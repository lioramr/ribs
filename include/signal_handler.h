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
#ifndef _SIGNAL_HANDLER__H_
#define _SIGNAL_HANDLER__H_

#include <sys/signalfd.h>
#include <signal.h>

struct signal_handler : basic_epoll_event
{
    int init(void (*cb)(), int sig)
    {
        callback = cb;
        this->method.set(&signal_handler::on_signal);
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, sig);
        
        this->fd = signalfd(-1, &set, SFD_NONBLOCK);
        if (0 > this->fd)
        {
            perror("signalfd");
            return -1;
        }
        return 0;
    }

    void init_per_thread()
    {
        epoll::add_multi(this);
    }

    struct basic_epoll_event *on_signal()
    {
        struct signalfd_siginfo siginfo;
        ssize_t n;
        do
        {
            n = ::read(this->fd, &siginfo, sizeof(siginfo));
            if (0 > n)
            {
                if (EAGAIN == errno)
                    return NULL;
                perror("signal read");
            } else
            {
                callback();
            }
        } while (n > 0);
        return NULL;
    }
    
    void (*callback)();
};

#endif // _SIGNALFD__H_
