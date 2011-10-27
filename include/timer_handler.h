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
#ifndef _TIMER_HANDLER__H_
#define _TIMER_HANDLER__H_

#include <sys/timerfd.h>

struct timer_handler : basic_epoll_event
{
    timer_handler() { this->fd = -1; }
    int init()
    {
        if (this->fd >= 0)
            return -1;
        method.set(&timer_handler::on_timer);
        this->fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
        if (0 > this->fd)
        {
            perror("timerfd_create");
            return -1;
        }
        return 0;
    }

    static int arm(int fd, uint64_t first_time, uint64_t interval)
    {
        struct itimerspec new_value = {{interval / 1000, (interval % 1000) * 1000000},{first_time / 1000, (first_time % 1000) * 1000000}};
        if (0 != timerfd_settime(fd, 0, &new_value, NULL))
        {
            perror("timerfd_settime");
            return -1;
        }
        return 0;
    }
    
    int arm(uint64_t first_time, uint64_t interval)
    {
        return arm(this->fd, first_time, interval);
    }
    
    void init_per_thread()
    {
        epoll::add_multi(this);
    }
    
    struct basic_epoll_event *on_timer()
    {
        uint64_t num_exp;
        if (sizeof(num_exp) == ::read(this->fd, &num_exp, sizeof(num_exp)))
            return callback.invoke(this);
        return NULL;
    }

    void close()
    {
        ::close(this->fd); // one time
        this->fd = -1;
    }

    //basic_epoll_event_callback_method_0arg callback;
    basic_epoll_event_callback_method_1arg<struct timer_handler *> callback;
};

#endif // _TIMER_HANDLER__H_
