/*
    This file is part of RIBS (Robust Infrastructure for Backend Systems).
    RIBS is an infrastructure for building great SaaS applications (but not
    limited to).

    Copyright (C) 2011 Adap.tv, Inc.

    RIBS is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, version 2.1 of the License.

    RIBS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with RIBS.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "http_client_file.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "sstr.h"
#include "logger.h"

SSTRL(CRLFCRLF, "\r\n\r\n");
SSTRL(CRLF, "\r\n");
SSTRL(CONTENT_LENGTH, "\r\nContent-Length: ");
SSTRL(TRANSFER_ENCODING, "\r\nTransfer-Encoding: ");

SSTRL(CONNECTION, "\r\nConnection: ");
SSTRL(CONNECTION_CLOSE, "close"); 

#define MAX_CHUNK_SIZE (1024*1024)

/* static */
struct http_client_file *http_client_file::s_clients = NULL;

/* static */
__thread http_client_file::persistent_clients_ht_t *http_client_file::ht_clients = NULL;

/* static */
void http_client_file::init()
{
    struct rlimit rl;
    if (0 > getrlimit(RLIMIT_NOFILE, &rl))
    {
        LOGGER_PERROR_STR("getrlimit");
        abort();
    }
            
    s_clients = new http_client_file[rl.rlim_cur];
    struct http_client_file *end = s_clients + rl.rlim_cur;
    int n = 0;
    for (struct http_client_file *c = s_clients; c != end; c->fd = n, ++c, ++n);
}

/* static */
struct http_client_file *http_client_file::create(const char *filename, struct in_addr *addr, uint16_t port)
{
    init_ht_clients();
    struct client_key k = { *addr, port, 0 };
    persistent_clients_ht_t::entry_t *e = ht_clients->lookup(k);
    if (NULL != e)
    {
        struct http_client_file *client = e->v;
        if (NULL != client)
        {
            struct http_client_file *p = client->next;
            if (NULL != p)
                p->prev = NULL;
            e->v = p;
            epoll::cancel_timeout(client);
            client->prepare();
            if (0 == client->infile.create(filename))
            {
                epoll::mod(client, EPOLLET|EPOLLOUT);
                return client;
            }
            client->close();
            return NULL;
        }
    }
    //printf("*** create (%hu / %u / %hu) ***\n", k.port, k.addr.s_addr, k.padding);
    return new_connection(filename, addr, port);
}

/* static */
struct http_client_file *http_client_file::new_connection(const char *filename, struct in_addr *addr, uint16_t port)
{
    //
    int cfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (0 > cfd)
    {
        LOGGER_PERROR_STR("socket");
        return NULL;
    }
    const int option = 1;
    if (0 > ::setsockopt(cfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)))
    {
        LOGGER_PERROR_STR("setsockopt SO_REUSEADDR");
    }

    if (0 > setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option)))
    {
        LOGGER_PERROR_STR("setsockopt TCP_NODELAY");
    }
        
    if (::fcntl(cfd, F_SETFL, O_NONBLOCK) == -1)
    {
        LOGGER_PERROR_STR("fcntl O_NONBLOCK");
    }
    
    //printf("*** new connection (%d) ***\n", cfd);
    
    struct http_client_file *client = s_clients + cfd;
    client->init_connection(addr, port);
    if (0 == client->infile.create(filename))
        return client;
    client->close();
    return NULL;
}

/* static */
int http_client_file::resolve_host_name(const char*host, struct in_addr &addr)
{
    hostent hent;
    int herror;
    char buf[16384];
    hostent *h;
    int res = gethostbyname_r(host, &hent, buf, sizeof(buf), &h, &herror);
    if (0 != res || NULL == h || NULL == (in_addr *)h->h_addr_list)
        return -1;
    addr = *(in_addr *)h->h_addr_list[0];
    return 0;

}

int http_client_file::prepare()
{
    this->method.set(&http_client_file::write_request);
    outbuf.init();
    inbuf.init();
    eoh = 0;
    persistent = 1; // assume HTTP/1.1
    return 0;
}

int http_client_file::init_connection(struct in_addr *addr, uint16_t port)
{
    key = (client_key_t) { *addr, port, 0 };
    prepare();
    
    this->connect(addr, port);
    
    epoll::add(this, EPOLLET|EPOLLOUT);
                
    return 0;
}

struct basic_epoll_event *http_client_file::write_request()
{
    int res = outbuf.write(fd);
    if (0 == res) // no error but didn't reach the end yet
        return epoll::yield(epoll::client_timeout_chain, this); // will go back to epoll_wait
    else if (0 > res) // error
    {
        LOGGER_PERROR_STR("writeRequest");
        persistent = 0;
        return callback.invoke(this);
    }
    // finished writing request
    this->method.set(&http_client_file::read_header);
    epoll::mod(this, EPOLLET|EPOLLIN);
    return this;
}

struct basic_epoll_event *http_client_file::read_header()
{
    if (inbuf.read(fd) <= 0)
        return report_error();

    *inbuf.wloc() = 0;
    // do we have the header?
    char * eohp = strstr(inbuf.data(), CRLFCRLF);
    if (NULL != eohp) // we have the entire header
    {
        eoh = eohp - inbuf.data() + SSTRLEN(CRLFCRLF);
        *eohp = 0; // terminate
        char *content_len_str = strstr(inbuf.data(), CONTENT_LENGTH);
        if (NULL != content_len_str)
        {
            content_len_str += SSTRLEN(CONTENT_LENGTH);
            content_length = atoi(content_len_str);
        } else
        {
            return report_error();
        }
        char *p = strstr(inbuf.data(), CONNECTION);
        if (p != NULL)
        {
            p += SSTRLEN(CONNECTION);
            persistent = (0 == SSTRNCMPI(CONNECTION_CLOSE, p) ? 0 : 1);
        }
        *eohp = CRLF[0]; // restore
        this->method.set(&http_client_file::read_content);
        infile.memcpy(inbuf.data(eoh), inbuf.wlocpos() - eoh); // move partial content to file
        return this; // switch to content mode
    } else
        return epoll::yield(epoll::client_timeout_chain, this);
}

struct basic_epoll_event *http_client_file::read_content()
{
    int res;
    if ((res = infile.read(fd)) < 0)
        return report_error();
    
    if (content_length <= infile.wlocpos())
    {
        infile.finalize();
        return callback.invoke(this); // we are done
    }
    if (0 == res) // disconnected
        return report_error();
    return epoll::yield(epoll::client_timeout_chain, this);
}

struct basic_epoll_event *http_client_file::handle_disconnect()
{
    if (next == NULL && prev == next)
    {
        persistent_clients_ht_t::entry_t *e = ht_clients->lookup(key);
        e->v = NULL;
    } else
    {
        if (NULL != next)
            next->prev = prev;
        if (NULL != prev)
            prev->next = next;
        else
        {
            persistent_clients_ht_t::entry_t *e = ht_clients->lookup(key);
            e->v = next;
        }
    }
    //printf("*** disconnect (%d) ***\n", fd);
    ::close(fd);
    return NULL;
}

struct basic_epoll_event *http_client_file::report_error()
{
    eoh = 0;
    persistent = 0;
    return callback.invoke(this);
}

void http_client_file::close()
{
    init_ht_clients();
    infile.free();
    if (persistent > 0)
    {
        this->method.set(&http_client_file::handle_disconnect);
        persistent_clients_ht_t::entry_t *e = ht_clients->lookup(key);
        if (e == NULL)
        {
            prev = next = NULL;
            ht_clients->insert(key, this);
        } else
        {
            if (NULL != e->v)
                e->v->prev = this;
            prev = NULL;
            next = e->v;
            e->v = this;
        }
        //printf("*** close (%hu / %u / %hu) ***\n", key.port, key.addr.s_addr, key.padding);
        // here we use the server timeout chain, since it is usually several seconds
        // compare to client which can be sub-second
        epoll::yield(epoll::server_timeout_chain, this);
    } else
    {
        //printf("*** close (%d) ***\n", fd);
        ::close(fd);
    }
}
