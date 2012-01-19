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
#ifndef _TCP_CLIENT__H_
#define _TCP_CLIENT__H_

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "basic_epoll_event.h"
#include "vmbuf.h"
#include "epoll.h"
#include "compact_hashtable.h"

template <typename T>
struct tcp_client : basic_epoll_event
{
    typedef struct client_key
    {
        struct in_addr addr;
        uint16_t port;
        uint16_t padding;
        bool operator==(const client_key &other) const
        {
            return (addr.s_addr == other.addr.s_addr) && (port == other.port);
        }
    } client_key_t;
    
    typedef compact_hashtable<client_key_t, struct tcp_client<T> *> persistent_clients_ht_t;
    static struct tcp_client<T> *&clients() { static struct tcp_client<T> *c = NULL; return c; };
    static persistent_clients_ht_t *&ht_clients() { static __thread persistent_clients_ht_t *h = NULL; return h; }
    
    static void init();

    static inline void init_ht_clients()
    {
        if (NULL == ht_clients())
        {
            ht_clients() = new persistent_clients_ht_t;
            ht_clients()->init(1024);
        }
    }

    static struct tcp_client<T> *create(struct in_addr addr, uint16_t port);
    static struct tcp_client<T> *new_connection(struct in_addr addr, uint16_t port);
    static int resolve_host_name(const char* host, struct in_addr &addr);

    int prepare();
    int connect(struct in_addr addr, uint16_t port);
    int init_connection(struct in_addr addr, uint16_t port);
    
    struct basic_epoll_event *write_request();
    struct basic_epoll_event *read_response();
    struct basic_epoll_event *handle_disconnect();

    void close();

    struct tcp_client<T> *next;
    struct tcp_client<T> *prev;
    vmbuf outbuf;
    vmbuf inbuf;
    int persistent;
    client_key_t key;
    union epoll_data user_data;
    
    basic_epoll_event_callback_method_1arg<struct tcp_client<T> *> callback;
    struct timeval timer_connect;
    T proto;
    
    void yield();
};

/* static */
template <typename T>
void tcp_client<T>::init()
{
    struct rlimit rl;
    if (0 > getrlimit(RLIMIT_NOFILE, &rl))
    {
        LOGGER_PERROR_STR("getrlimit");
        abort();
    }
            
    clients() = new tcp_client<T>[rl.rlim_cur];
    tcp_client<T> *end = clients() + rl.rlim_cur;
    int n = 0;
    for (struct tcp_client<T> *c = clients(); c != end; c->fd = n, ++c, ++n);
}

template <typename T>
inline int tcp_client<T>::connect(struct in_addr addr, uint16_t port)
{
    struct sockaddr_in server;
    server.sin_port = htons(port);
    server.sin_family = AF_INET;
    server.sin_addr = addr;
    return ::connect(fd, (sockaddr *)&server, sizeof(server));
}

template <typename T>
inline void tcp_client<T>::yield()
{
    epoll::yield(epoll::client_timeout_chain, this);
}

/* static */
template <typename T>
inline struct tcp_client<T> *tcp_client<T>::create(struct in_addr addr, uint16_t port)
{
    init_ht_clients();
    struct client_key k = { addr, port, 0 };
    typename tcp_client<T>::persistent_clients_ht_t::entry_t *e = ht_clients()->lookup(k);
    if (NULL != e)
    {
        struct tcp_client<T> *client = e->v;
        if (NULL != client)
        {
            struct tcp_client<T> *p = client->next;
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
template <typename T>
inline struct tcp_client<T> *tcp_client<T>::new_connection(struct in_addr addr, uint16_t port)
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
    
    struct tcp_client<T> *client = clients() + cfd;
    client->init_connection(addr, port);
    return client;
}

template <typename T>
inline int tcp_client<T>::init_connection(struct in_addr addr, uint16_t port)
{
    key = (client_key_t) { addr, port, 0 };
    prepare();
    
    this->connect(addr, port);
    
    epoll::add(this, EPOLLET|EPOLLOUT);
                
    return 0;
}

/* static */
template <typename T>
inline int tcp_client<T>::resolve_host_name(const char*host, struct in_addr &addr)
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

template <typename T>
inline int tcp_client<T>::prepare()
{
    this->method.set(&tcp_client<T>::write_request);
    outbuf.init();
    inbuf.init();
    persistent = 1; // assume persistent
    gettimeofday(&timer_connect, NULL);
    return T::prepare(this);
}

template <typename T>
inline struct basic_epoll_event *tcp_client<T>::write_request()
{
    int res = outbuf.write(fd);
    if (0 == res) // no error but didn't reach the end yet
        return epoll::yield(epoll::client_timeout_chain, this); // will go back to epoll_wait
    else if (0 > res) // error
    {
        LOGGER_PERROR_STR("writeRequest");
        persistent = 0;
        T::on_error(this);
        return callback.invoke(this);
    }
    // finished writing request
    this->method.set(&tcp_client<T>::read_response);
    epoll::mod(this, EPOLLET|EPOLLIN);
    return this;
}

template <typename T>
inline struct basic_epoll_event *tcp_client<T>::read_response()
{
    int res = inbuf.read(fd);
    if (0 >= res) // error or remote side closed connection
    {
        if (0 > res)
            T::on_error(this);
        else
            T::on_connection_close(this);
        
        persistent = 0;
        return callback.invoke(this);
    }
    if (0 < T::read_content(this))
        return epoll::yield(epoll::client_timeout_chain, this);
    return callback.invoke(this);
}

template <typename T>
inline void tcp_client<T>::close()
{
    init_ht_clients();
    if (persistent > 0)
    {
        this->method.set(&tcp_client<T>::handle_disconnect);
        typename persistent_clients_ht_t::entry_t *e = ht_clients()->lookup(key);
        if (e == NULL)
        {
            prev = next = NULL;
            ht_clients()->insert(key, this);
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


template <typename T>
inline struct basic_epoll_event *tcp_client<T>::handle_disconnect()
{
    if (next == NULL && prev == next)
    {
        typename persistent_clients_ht_t::entry_t *e = ht_clients()->lookup(key);
        e->v = NULL;
    } else
    {
        if (NULL != next)
            next->prev = prev;
        if (NULL != prev)
            prev->next = next;
        else
        {
            typename persistent_clients_ht_t::entry_t *e = ht_clients()->lookup(key);
            e->v = next;
        }
    }
    //printf("*** disconnect (%d) ***\n", fd);
    ::close(fd);
    return NULL;
}

#endif // _TCP_CLIENT__H_
