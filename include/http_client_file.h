#ifndef _HTTP_CLIENT_FILE__H_
#define _HTTP_CLIENT_FILE__H_

#include "basic_epoll_event.h"
#include <unistd.h>
#include <stdlib.h>
#include "vmbuf.h"
#include "epoll.h"
#include "sstr.h"
#include "compact_hashtable.h"
#include <netinet/in.h>

struct http_client_file : basic_epoll_event
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
    
    typedef compact_hashtable<client_key_t, struct http_client_file *> persistent_clients_ht_t;
    static struct http_client_file *s_clients;
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
    
    static struct http_client_file *create(const char *filename, struct in_addr *addr, uint16_t port);
    static struct http_client_file *new_connection(const char *filename, struct in_addr *addr, uint16_t port);
    static int resolve_host_name(const char*host, struct in_addr &addr);

    int prepare();
    int connect(struct in_addr *addr, uint16_t port);
    int init_connection(struct in_addr *addr, uint16_t port);
    
    struct basic_epoll_event *write_request();
    struct basic_epoll_event *read_header();
    struct basic_epoll_event *read_content();
    struct basic_epoll_event *handle_disconnect();
    struct basic_epoll_event *report_error();

    void close();

    struct http_client_file *next;
    struct http_client_file *prev;
    vmbuf outbuf;
    vmbuf inbuf;
    vmfile infile;
    uint32_t eoh; // end of header
    uint32_t content_length;
    int persistent;
    client_key_t key;
    
    basic_epoll_event_callback_method_1arg<struct http_client_file *> callback;

    int http_return_code()
    {
        SSTRL(HTTP, "HTTP/");
        if (0 != SSTRNCMP(HTTP, inbuf.data()))
            return 500;
        char *p = strchrnul(inbuf.data(), ' ');
        return (*p ? atoi(p + 1) : 500);
    }
    int fetch_uri(const char *URI)
    {
        outbuf.sprintf("GET %s HTTP/1.1\r\nHost: %s\r\n\r\n",
                       URI,
                       "localhost");
        return 0;
    }
    void yield();
};

inline int http_client_file::connect(struct in_addr *addr, uint16_t port)
{
    struct sockaddr_in server;
    server.sin_port = htons(port);
    server.sin_family = AF_INET;
    server.sin_addr = *addr;
    return ::connect(fd, (sockaddr *)&server, sizeof(server));
}

inline void http_client_file::yield()
{
    epoll::yield(epoll::client_timeout_chain, this);
}


#endif // _HTTP_CLIENT_FILE__H_
