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
#ifndef _EPOLL_EVENT_ARRAY__H_
#define _EPOLL_EVENT_ARRAY__H_

#include "basic_epoll_event.h"
#include "ptr_array.h"
#include <sys/time.h>
#include <sys/resource.h>

template<typename T>
struct epoll_event_array_template : ptr_array<T>
{
    template<typename F>
    void init(const F &f)
    {
        struct rlimit rlim;
        ::getrlimit(RLIMIT_NOFILE, &rlim);
        ptr_array<T>::init((size_t)rlim.rlim_cur, f);
        int fd = 0;
        for (T **ev = ptr_array<T>::begin(), **evEnd = ptr_array<T>::end(); ev != evEnd; ++ev, ++fd)
            (*ev)->fd = fd;
    }
};

struct epoll_event_array : epoll_event_array_template<struct basic_epoll_event>
{
};

struct epoll_server_event_array : epoll_event_array_template<struct server_epoll_event>
{
};


#endif // _EPOLL_EVENT_ARRAY__H_
