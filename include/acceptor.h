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
    // N.Y. backward compatibility interface, can go away
    int init(int port, int listen_backlog, struct epoll_event_array *events)
    {
        return init(-1, port, listen_backlog, events);
    }
    
    int init(int fd, int port, int listen_backlog, struct epoll_event_array *events)
    {
        this->events = events;
        return _init(fd, port, listen_backlog);
    }
    
    int _init(int fd, int port, int listen_backlog);
           
    struct basic_epoll_event *on_accept();
    
    void init_per_thread() { epoll::add_multi(this, EPOLLIN); }
    
    struct epoll_event_array *events;
    
    struct basic_epoll_event_method_0args callback;
};

#include "../src/acceptor.cpp" // inline 

#endif // _ACCEPTOR__H_
