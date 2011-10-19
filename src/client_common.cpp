#include "client_common.h"
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "logger.h"

int resolve_host_name(const char*host, struct in_addr &addr)
{
    hostent hent;
    int herror;
    char buf[16384];
    hostent *h;
    int res = gethostbyname_r(host, &hent, buf, sizeof(buf), &h, &herror);
    if (0 != res || NULL == h || NULL == (in_addr *)h->h_addr_list)
        return -1;
    addr = *(in_addr *)h->h_addr_list[0];
    return 0;
}

int parse_ip_and_port(const char *host, struct sockaddr_in *addr)
{
    char host_copy[strlen(host) + 1];
    strcpy(host_copy, host);
    
    char *p = strchrnul(host_copy, ':');
    addr->sin_port = 80;
    if (*p)
    {
        *p++ = 0;
        addr->sin_port = atoi(p);
    }
    if (0 > resolve_host_name(host_copy, addr->sin_addr))
    {
        logger::log("ERROR: failed to resolve host name: %s", host_copy);
        return -1;
    }
    return 0;
}

