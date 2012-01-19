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
#include "client_common.h"
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "logger.h"

int resolve_host_name(const char*host, struct in_addr *addr)
{
    hostent hent;
    int herror;
    char buf[16384];
    hostent *h;
    int res = gethostbyname_r(host, &hent, buf, sizeof(buf), &h, &herror);
    if (0 != res || NULL == h || NULL == (in_addr *)h->h_addr_list)
        return -1;
    *addr = *(in_addr *)h->h_addr_list[0];
    return 0;
}

int parse_ip_and_port(const char *host, struct in_addr *addr, uint16_t *port)
{
    char host_copy[strlen(host) + 1];
    strcpy(host_copy, host);

    char *p = strchrnul(host_copy, ':');
    *port = 80;
    if (*p)
    {
        *p++ = 0;
        *port = atoi(p);
    }
    if (0 > resolve_host_name(host_copy, addr))
    {
        logger::log("ERROR: failed to resolve host name: %s", host_copy);
        return -1;
    }
    return 0;
}

