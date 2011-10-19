#include "http_client.h"
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
struct http_client *http_client::s_clients = NULL;

/* static */
__thread http_client::persistent_clients_ht_t *http_client::ht_clients = NULL;

/* static */
void http_client::init()
{
    struct rlimit rl;
    if (0 > getrlimit(RLIMIT_NOFILE, &rl))
    {
        LOGGER_PERROR_STR("getrlimit");
        abort();
    }
            
    s_clients = new http_client[rl.rlim_cur];
    struct http_client *end = s_clients + rl.rlim_cur;
    int n = 0;
    for (struct http_client *c = s_clients; c != end; c->fd = n, ++c, ++n);
}

/* static */
struct http_client *http_client::create(struct in_addr *addr, uint16_t port)
{
    init_ht_clients();
    struct client_key k = { *addr, port, 0 };
    persistent_clients_ht_t::entry_t *e = ht_clients->lookup(k);
    if (NULL != e)
    {
        struct http_client *client = e->v;
        if (NULL != client)
        {
            struct http_client *p = client->next;
            if (NULL != p)
                p->prev = NULL;
            e->v = p;
            epoll::cancel_timeout(client);
            client->prepare();
            epoll::mod(client, EPOLLET|EPOLLOUT);
            //printf("*** lookup (%d) ***\n", client->fd);
            return client;
        }
    }
    //printf("*** create (%hu / %u / %hu) ***\n", k.port, k.addr.s_addr, k.padding);
    return new_connection(addr, port);
}

/* static */
struct http_client *http_client::new_connection(struct in_addr *addr, uint16_t port)
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
    
    struct http_client *client = s_clients + cfd;
    client->init_connection(addr, port);
    return client;
}

/* static */
int http_client::resolve_host_name(const char*host, struct in_addr &addr)
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

int http_client::prepare()
{
    this->method.set(&http_client::write_request);
    outbuf.init();
    inbuf.init();
    eoh = 0;
    persistent = 1; // assume HTTP/1.1
    chunked = -1;
    chunk_start = 0;
    gettimeofday(&timer_connect, NULL);
    return 0;
}

int http_client::init_connection(struct in_addr *addr, uint16_t port)
{
    key = (client_key_t) { *addr, port, 0 };
    prepare();
    chunked = -1;
    
    this->connect(addr, port);
    
    epoll::add(this, EPOLLET|EPOLLOUT);
                
    return 0;
}

struct basic_epoll_event *http_client::write_request()
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
    this->method.set(&http_client::read_response);
    epoll::mod(this, EPOLLET|EPOLLIN);
    return this;
}

struct basic_epoll_event *http_client::read_response()
{
    int res = inbuf.read(fd);
    if (0 >= res) // error or remote side closed connection
    {
        if (0 > res || (0 < read_content() && eoh > 0 && chunked >= 0))
            eoh = 0; // error or partial response. we use eoh == 0 to detect problems
        
        persistent = 0;
        return callback.invoke(this);
    }
    if (0 < read_content())
        return epoll::yield(epoll::client_timeout_chain, this);
    return callback.invoke(this);
}

int http_client::chunk_size()
{
    if (0 != chunk_end)
        return 0;  // already reading a chunk
    *inbuf.wloc() = 0;
    char *the_size = inbuf.data() + chunk_start;
    char *p = strchrnul(the_size, '\r');
    if (*p)
    {
        uint32_t s = strtoul(the_size, NULL, 16);
        chunk_start = p - inbuf.data() + SSTRLEN(CRLF);
        chunk_end = chunk_start + s +  SSTRLEN(CRLF);
    } else
        return 1; // wait for more data
    return 0;
}

int http_client::handle_chunks()
{
    for (;;)
    {
        if (chunk_size() > 0) // do we have the chunk size?
            break;
        
        if (chunk_end <= inbuf.wlocpos())
        {
            if (chunk_start + SSTRLEN(CRLF) == chunk_end) // no more chunks
                return 0; // we are done
            chunk_start = chunk_end;
            chunk_end = 0;
            continue; // next chunk
        }
        break;
    }
    return 1; // not in last chunk yet, yield
}

struct basic_epoll_event *http_client::handle_disconnect()
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

void http_client::close()
{
    init_ht_clients();
    if (persistent > 0)
    {
        this->method.set(&http_client::handle_disconnect);
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
        //printf("*** standby (%d, %hu / %u / %hu) ***\n", fd, key.port, key.addr.s_addr, key.padding);
        // here we use the server timeout chain, since it is usually several seconds
        // compare to client which can be sub-second
        epoll::yield(epoll::server_timeout_chain, this);
    } else
    {
        //printf("*** close (%d) ***\n", fd);
        ::close(fd);
    }
}

int http_client::read_content()
{
    if (0 == eoh) // first time case
    {
        *inbuf.wloc() = 0;
        // do we have the header?
        char * eohp = strstr(inbuf.data(), CRLFCRLF);
        if (NULL != eohp) // we have the entire header
        {
            eoh = eohp - inbuf.data() + SSTRLEN(CRLFCRLF);
            chunk_start = eoh;
            *eohp = 0; // terminate

            char *p = strstr(inbuf.data(), CONNECTION);
            if (p != NULL)
            {
                p += SSTRLEN(CONNECTION);
                persistent = (0 == SSTRNCMPI(CONNECTION_CLOSE, p) ? 0 : 1);
            }

            // is 204 or 304?
            SSTRL(HTTP, "HTTP/");
            if (0 != SSTRNCMP(HTTP, inbuf.data()))
            {
                eoh = 0;
                return 0;
            }
            p = strchrnul(inbuf.data(), ' ');
            int code = (*p ? atoi(p + 1) : 0);
            if (code == 204 || code == 304)
                return 0;
            if (code == 0)
            {
                eoh = 0;
                persistent = 0;
                return 0;
            }
            
            char *content_len_str = strstr(inbuf.data(), CONTENT_LENGTH);
            if (NULL != content_len_str)
            {
                chunked = 0;
                content_len_str += SSTRLEN(CONTENT_LENGTH);
                chunk_end = chunk_start + atoi(content_len_str);
            } else
            {
                char *transfer_encoding_str = strstr(inbuf.data(), TRANSFER_ENCODING);
                if (NULL != transfer_encoding_str &&
                    0 == SSTRNCMP(transfer_encoding_str + SSTRLEN(TRANSFER_ENCODING), "chunked"))
                {
                    chunked = 1;
                    chunk_end = 0;
                } else
                    chunked = -1;
            }
            
            *eohp = CRLF[0]; // restore
        } else
            return 1; // no header yet, yield
    }

    // manage state
    switch (chunked)
    {
    case -1:
        return 1; // chunk size unknown, always yield
    case 0:
        if (chunk_end <= inbuf.wlocpos())
            return 0; // we are done
        break;
    case 1:
        return http_client::handle_chunks();
    }
    return 1; // yield
}

int http_client::next_chunk(chunk *chunk)
{
    uint32_t s, loc;
    
    switch (chunked)
    {
    case -1:
        s = inbuf.wlocpos() - eoh;
        break;
    case 0:
        s = chunk_end - chunk_start;
        break;
    case 1:
        if (0 == chunk->loc)
            loc = eoh;
        else
            loc = chunk->loc + chunk->size + SSTRLEN(CRLF);
        {
            char *p = inbuf.data() + loc;
            char *ep;
            chunk->size = strtoul(p, &ep, 16);
            loc = ep - inbuf.data() + SSTRLEN(CRLF);
            chunk->loc = loc;
        }
        return chunk->size == 0 ? -1 : 0;
    default:
        s = 0;
        break;
    }
    if (0 == chunk->loc)
    {
        chunk->loc = eoh;
        chunk->size = s;
    } else
        return -1;
    
    return 0;
}
