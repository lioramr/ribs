#ifndef _ACCEPTOR__H_
#define _ACCEPTOR__H_

#include "basic_epoll_event.h"
#include "epoll_event_array.h"
#include "epoll.h"
#include "class_factory.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

template<typename T>
struct acceptor : basic_epoll_event
{
    int init(int port, int listen_backlog, struct epoll_event_array *events)
    {
        this->events = events;
        return _init(port, listen_backlog);
    }
    
    int _init(int port, int listen_backlog);
           
    struct basic_epoll_event *on_accept();
    
    void init_per_thread() { epoll::add_multi(this, EPOLLIN); }
    
    struct epoll_event_array *events;
    
    struct basic_epoll_event_method_0args callback;
};

#include "../src/acceptor.cpp" // inline 

#endif // _ACCEPTOR__H_
