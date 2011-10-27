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
#ifndef _HTTP_CLIENT__H_
#define _HTTP_CLIENT__H_

#include "basic_epoll_event.h"
#include <unistd.h>
#include "vmbuf.h"
#include "epoll.h"
#include "compact_hashtable.h"
#include <netinet/in.h>


struct http_client : basic_epoll_event
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
    
    typedef compact_hashtable<client_key_t, struct http_client *> persistent_clients_ht_t;
    static struct http_client *s_clients;
    static __thread persistent_clients_ht_t *ht_clients;
    
    static void init();

    static inline void init_ht_clients()
    {
        if (NULL == ht_clients)
        {
            ht_clients = new persistent_clients_ht_t;
            ht_clients->init(1024);
        }
    }
    
    static struct http_client *create(struct in_addr *addr, uint16_t port);
    static struct http_client *new_connection(struct in_addr *addr, uint16_t port);
    static int resolve_host_name(const char*host, struct in_addr &addr);

    int prepare();
    int connect(struct in_addr *addr, uint16_t port);
    int init_connection(struct in_addr *addr, uint16_t port);
    
    struct basic_epoll_event *write_request();
    struct basic_epoll_event *read_response();
    struct basic_epoll_event *handle_disconnect();
    int read_content();
    int chunk_size();
    int handle_chunks();

    void close();

    struct http_client *next;
    struct http_client *prev;
    vmbuf outbuf;
    vmbuf inbuf;
    uint32_t eoh;
    uint32_t chunk_start;
    uint32_t chunk_end;
    int chunked;
    int persistent;
    client_key_t key;
    union epoll_data user_data;
    
    basic_epoll_event_callback_method_1arg<struct http_client *> callback;
    struct timeval timer_connect;
    
    struct chunk
    {
        uint32_t loc;
        uint32_t size;
        chunk() : loc(0), size(0) {}
    };
    
    int next_chunk(chunk *chunk);
    char *get_chunk(chunk *chunk) { return inbuf.data() + chunk->loc; }
    uint32_t get_chunk_size(chunk *chunk) { return chunk->size; }

    void yield();
};

inline int http_client::connect(struct in_addr *addr, uint16_t port)
{
    struct sockaddr_in server;
    server.sin_port = htons(port);
    server.sin_family = AF_INET;
    server.sin_addr = *addr;
    return ::connect(fd, (sockaddr *)&server, sizeof(server));
}

inline void http_client::yield()
{
    epoll::yield(epoll::client_timeout_chain, this);
}

#endif // _HTTP_CLIENT__H_
