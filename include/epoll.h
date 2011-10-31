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
#ifndef _EPOLL__H_
#define _EPOLL__H_

#include <sys/epoll.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include "logger.h"
#include "basic_epoll_event.h"

struct epoll
{
    enum
    {
        DEFAULT_SERVER_TIMEOUT = 5, // seconds
        DEFAULT_CLIENT_TIMEOUT = 1000 // milli-seconds
    };

    typedef int (*callback_t)();

    static __thread int epollfd;
    static callback_t per_thread_callback;
    static __thread void *label_run;
    static __thread void *label_done;

    static __thread struct basic_epoll_event *server_timeout_chain;
    static __thread struct basic_epoll_event *client_timeout_chain;

    static struct timeval server_timeout;
    static struct timeval client_timeout;

    static int init(time_t to_server, time_t to_client);
    static void *thread_main(void *);
    static void *run(void *);

    static int start(int num_threads = 0);
    static void stop() { label_run = label_done; }

    static void set_per_thread_callback(callback_t cb);
    static int mask_signals();

    static inline int arm_timeout_timer(int fd, struct timeval *tv);

    static inline int ctl(struct basic_epoll_event *e, int op, uint32_t events);
    static inline int add(struct basic_epoll_event *e, uint32_t events = EPOLLET|EPOLLIN|EPOLLOUT);
    static inline int del(struct basic_epoll_event *e);
    static inline int mod(struct basic_epoll_event *e, uint32_t events = EPOLLET|EPOLLIN|EPOLLOUT);
    static inline int add_multi(struct basic_epoll_event *e, uint32_t events = EPOLLIN);

    static inline struct basic_epoll_event *yield(struct basic_epoll_event *timeout_chain, struct basic_epoll_event *e);

    static inline void cancel_timeout(struct basic_epoll_event *e);
    static inline void schedule_timeout(struct basic_epoll_event *timeout_chain, struct basic_epoll_event *e);
    static inline void handle_timeouts(struct basic_epoll_event *timeout_chain);
};

/*
 * inline functions
 */

/* static */
inline int epoll::arm_timeout_timer(int fd, struct timeval *tv)
{
    struct itimerspec new_value = {{0, 0}, {tv->tv_sec, tv->tv_usec * 1000}};
    if (0 != timerfd_settime(fd, 0, &new_value, NULL))
    {
        LOGGER_PERROR_STR("timerfd_settime");
        return -1;
    }
    return 0;
}


/* static */
inline int epoll::ctl(struct basic_epoll_event *e, int op, uint32_t events)
{
    epoll_event ev;
    ev.events = events;
    ev.data.ptr = e;
    if (0 > epoll_ctl(epoll::epollfd, op, e->fd, &ev))
    {
        LOGGER_PERROR_STR("epoll_ctl");
        ::close(e->fd);
        return -1;
    }
    return 0;
}

/* static */
inline int epoll::add(struct basic_epoll_event *e, uint32_t events /* = EPOLLET|EPOLLIN|EPOLLOUT */)
{
    return ctl(e, EPOLL_CTL_ADD, events);
}

/* static */
inline int epoll::del(struct basic_epoll_event *e)
{
    if (0 > epoll_ctl(epoll::epollfd, EPOLL_CTL_DEL, e->fd, NULL))
        return -1;
    return 0;
}

/* static */
inline int epoll::mod(struct basic_epoll_event *e, uint32_t events /* = EPOLLET|EPOLLIN|EPOLLOUT */)
{
    return ctl(e, EPOLL_CTL_MOD, events);
}

/* static */
inline int epoll::add_multi(struct basic_epoll_event *e, uint32_t events /* = EPOLLIN */)
{
    timerclear(&e->last_event_ts);
    return ctl(e, EPOLL_CTL_ADD, events);
}

/* static */
inline struct basic_epoll_event *epoll::yield(struct basic_epoll_event *timeout_chain, struct basic_epoll_event *e)
{
    schedule_timeout(timeout_chain, e);
    return NULL;
}

/* static */
inline void epoll::cancel_timeout(struct basic_epoll_event *e)
{
    if (0 != timerisset(&e->last_event_ts))
    {
        // remove from chain
        e->timeout_chain_prev->timeout_chain_next = e->timeout_chain_next;
        e->timeout_chain_next->timeout_chain_prev = e->timeout_chain_prev;
    }
}

/* static */
inline void epoll::schedule_timeout(struct basic_epoll_event *timeout_chain, struct basic_epoll_event *e)
{
    gettimeofday(&e->last_event_ts, NULL);
    if (timeout_chain == timeout_chain->timeout_chain_next) // arm the timer
        arm_timeout_timer(timeout_chain->fd, &timeout_chain->last_event_ts);

    // add to chain
    e->timeout_chain_next = timeout_chain;
    e->timeout_chain_prev = timeout_chain->timeout_chain_prev;
    timeout_chain->timeout_chain_prev->timeout_chain_next = e;
    timeout_chain->timeout_chain_prev = e;
}

/* static */
inline void epoll::handle_timeouts(struct basic_epoll_event *timeout_chain)
{
    struct basic_epoll_event *p = timeout_chain->timeout_chain_next;
    struct timeval now, ts;
    gettimeofday(&now, NULL);
    timersub(&now, &timeout_chain->last_event_ts, &ts);
    for (;;)
    {
        if (p == p->timeout_chain_next || timercmp(&p->last_event_ts, &ts, >))
            break;
        if (0 > ::shutdown(p->fd, SHUT_RDWR))
            LOGGER_PERROR("shutdown [%d]", p->fd);
        // make sure not to cancel it again when the fd wakes up in the epoll loop
        timerclear(&p->last_event_ts); 
        timeout_chain->timeout_chain_next = p->timeout_chain_next;
        p = timeout_chain->timeout_chain_next;
        p->timeout_chain_prev = timeout_chain;
    }
    if (p != p->timeout_chain_next)
    {
        timersub(&p->last_event_ts, &ts, &now);
        arm_timeout_timer(timeout_chain->fd, &now);
    }
}




#endif // _EPOLL__H_
