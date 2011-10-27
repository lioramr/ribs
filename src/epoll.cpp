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
#include "epoll.h"
#include <pthread.h>
#include <stdlib.h>

/* static */
__thread int epoll::epollfd = -1;
/* static */
epoll::callback_t epoll::per_thread_callback = NULL;

/* static */
__thread struct basic_epoll_event *epoll::server_timeout_chain;
/* static */
__thread struct basic_epoll_event *epoll::client_timeout_chain;

/* static */
struct timeval epoll::server_timeout = { epoll::DEFAULT_SERVER_TIMEOUT, 0 };
/* static */
struct timeval epoll::client_timeout = { epoll::DEFAULT_CLIENT_TIMEOUT, 0 };

struct epoll_timeout_handler : basic_epoll_event
{
    void init(struct basic_epoll_event *chain)
    {
        to_chain = chain;
        method.set(&epoll_timeout_handler::on_timer);
        this->fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
        if (0 > this->fd)
            LOGGER_PERROR_STR("timerfd_create");
        epoll::add_multi(this);
    }

    struct basic_epoll_event *on_timer()
    {
        uint64_t num_exp;
        if (sizeof(num_exp) == ::read(this->fd, &num_exp, sizeof(num_exp)))
            epoll::handle_timeouts(to_chain);
        return NULL;
    }
    struct basic_epoll_event *to_chain;
};

/* static */
int epoll::init(time_t to_server, time_t to_client)
{
    epollfd = epoll_create(1); // size is ignored but must be positive
    if (0 > epollfd)
        return LOGGER_PERROR_STR("epoll_create"), -1;
    server_timeout = (struct timeval) { to_server, 0 };
    client_timeout = (struct timeval) { to_client / 1000,  (to_client % 1000) * 1000 };
    return 0;
}

/* static */
void *epoll::thread_main(void *arg)
{
    epollfd = epoll_create(1); // size is ignored but must be positive
    if (0 > epollfd)
        return LOGGER_PERROR_STR("epoll_create"), (void *)NULL;
    return run(arg);
}

/* static */
void * epoll::run(void *)
{
    struct basic_epoll_event server_to_chain;
    timerclear(&server_to_chain.last_event_ts);
    server_timeout_chain = &server_to_chain;
    server_timeout_chain->timeout_chain_next = server_timeout_chain;
    server_timeout_chain->timeout_chain_prev = server_timeout_chain;

    struct basic_epoll_event client_to_chain;
    timerclear(&client_to_chain.last_event_ts);
    client_timeout_chain = &client_to_chain;
    client_timeout_chain->timeout_chain_next = client_timeout_chain;
    client_timeout_chain->timeout_chain_prev = client_timeout_chain;

    if (NULL != per_thread_callback)
        if (0 > per_thread_callback())
            return NULL;
    epoll_timeout_handler server_to_handler;
    server_to_handler.init(epoll::server_timeout_chain);
    server_timeout_chain->fd = server_to_handler.fd;
    server_timeout_chain->last_event_ts = epoll::server_timeout;
    
    epoll_timeout_handler client_to_handler;
    client_to_handler.init(epoll::client_timeout_chain);
    client_timeout_chain->fd = client_to_handler.fd;
    client_timeout_chain->last_event_ts = epoll::client_timeout;
    
    struct epoll_event epollev;
    while (1)
    {
        if (0 >= epoll_wait(epollfd, &epollev, 1, -1))
            continue;
        struct basic_epoll_event *e = (basic_epoll_event *)epollev.data.ptr;
        epoll::cancel_timeout(e);
        while (NULL != (e = e->invoke()));
    }
    return NULL;
}

/* static */
void epoll::start(int num_threads /* = 0 */)
{
    if (num_threads <= 0)
    {
        // auto-detect cores
        num_threads = sysconf(_SC_NPROCESSORS_ONLN);
    }
    LOGGER_INFO_AT("creating %d worker threads", num_threads);
    --num_threads;
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; ++i)
        pthread_create(threads + i, NULL, epoll::thread_main, NULL);
    run(NULL);
    for (int i = 0; i < num_threads; ++i)
        pthread_join(threads[i], NULL);
}

/* static */
void epoll::set_per_thread_callback(callback_t cb)
{
    per_thread_callback = cb;
}

