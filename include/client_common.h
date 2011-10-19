#ifndef _CLIENT_COMMON__H_
#define _CLIENT_COMMON__H_

int resolve_host_name(const char*host, struct in_addr &addr);
int parse_ip_and_port(const char *host, struct sockaddr_in *addr);
    
#endif // _CLIENT_COMMON__H_
