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
//#include "acceptor.h"
#include "logger.h"

template<typename T>
inline int acceptor<T>::_init(int port, int listen_backlog)
{    
    this->method.set(&acceptor::on_accept);
    // create listener
    int listenfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (0 > listenfd)
        return -1;
    
    int rc;
    rc = fcntl(listenfd, F_SETFL, O_NONBLOCK);
    if (0 > rc)
        return rc;
        
    const int option = 1;
    rc = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    if (0 > rc)
    {
        perror("setsockopt, SO_REUSEADDR");
        return rc;
    }
        
    rc = setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option));
    if (0 > rc)
    {
        perror("setsockopt, TCP_NODELAY");
        return rc;
    }
        
    struct linger ls;
    ls.l_onoff = 0;
    ls.l_linger = 0;
    rc = setsockopt(listenfd, SOL_SOCKET, SO_LINGER, (void *)&ls, sizeof(ls));
    if (0 > rc)
    {
        perror("setsockopt, SO_LINGER");
        return rc;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (0 > ::bind(listenfd, (sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind");
        return -1;
    }

    if (0 > listen(listenfd, listen_backlog))
    {
        perror("listen");
        return -1;
    }
    LOGGER_INFO_AT("listening on port: %d, backlog: %d", port, listen_backlog);
    this->fd = listenfd;
    return 0;
}

template<typename T>
struct basic_epoll_event *acceptor<T>::on_accept()
{
    struct sockaddr_in new_addr;
    socklen_t new_addr_size = sizeof(struct sockaddr_in);
    int acceptfd = accept4(this->fd, (sockaddr *)&new_addr, &new_addr_size, SOCK_NONBLOCK);
    if (0 > acceptfd)
        return NULL;
    
    struct basic_epoll_event *event = events->get(acceptfd);
    epoll::add(event, EPOLLET | EPOLLIN | EPOLLOUT);
    ((T *)event)->callback = callback;
    return event; 
}
