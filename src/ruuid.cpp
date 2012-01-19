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
#include "ruuid.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <netdb.h>
#include <string.h>
#include <xmmintrin.h>
#include <stdio.h>

union uuid_data
{
    uint8_t bytes[16];
    struct
    {
        uint8_t addr[6];
        uint16_t tid;
        uint64_t tsc;
    } fields;
    uint64_t m64[2];
};

static __thread pid_t stored_tid;
static uint8_t stored_addr[6];

static inline uint64_t rdtsc()
{
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return (uint64_t)hi << 32 | lo;
}

static int get_mac_addr(uint8_t *bytes)
{
    ifreq ifr;
    ifconf ifc;
    char buf[1024];
    uint8_t zeros[] = { 0, 0, 0, 0, 0, 0 };

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (0 > sock)
        return -1;

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
        return ::close(sock), -1;
    
    ifreq* it = ifc.ifc_req;
    const ifreq* const end = it + (ifc.ifc_len / sizeof(ifreq));
    int ret = -1;
    for (; it != end; ++it)
    {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (0 == ioctl(sock, SIOCGIFFLAGS, &ifr))
        {
            if (0 == (ifr.ifr_flags & IFF_LOOPBACK) &&
                0 == ioctl(sock, SIOCGIFHWADDR, &ifr) &&
                0 != memcmp(ifr.ifr_addr.sa_data, zeros, sizeof(zeros)))
            {
                memcpy(bytes, ifr.ifr_addr.sa_data, 6);
                ret = 0;
                break;
            }
        }
    }
    close(sock);
    return ret;
}

int uuid_init()
{
    if (0 > get_mac_addr(stored_addr))
        return -1;
    stored_tid = (pid_t) syscall (SYS_gettid);
    return 0;
}

int uuid_init_thread()
{
    stored_tid = (pid_t) syscall (SYS_gettid);
    return 0;
}

int uuid_generate(struct uuid *bytes)
{
    union uuid_data *u = (union uuid_data *)bytes;
    u->fields.tsc = rdtsc();
    u->fields.tid = stored_tid;
    memcpy(u->fields.addr, stored_addr, 6);
    return 0;
}

inline void byte_to_hex_str(uint8_t b, char *buf)
{
    static char digits[] = "0123456789ABCDEF";
    *buf++ = digits[b >> 4];
    *buf = digits[b & 0x0F];
}

inline void uuid_2_buf(struct uuid *uuid, char *p)
{
    uint8_t *u = uuid->bytes;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p); p+=2;
    byte_to_hex_str(*u++, p);
}


void uuid_2_str(struct uuid *uuid, uuid_str *uuid_str)
{
    uuid_2_buf(uuid, uuid_str->str);
    uuid_str->str[32] = 0;
}

void uuid_2_vmbuf(struct uuid *uuid, vmbuf *buf)
{
    buf->resize_if_less(32);
    uuid_2_buf(uuid, buf->wloc());
    buf->wseek(32);
    *buf->wloc() = 0;
}

inline uint8_t hex_to_num(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

int uuid_parse(struct uuid *uuid, const char *str)
{
    uint8_t *u = uuid->bytes;
    uint8_t *end = u + 16;
    while (*str)
    {
        if (u == end)
            return -1;
        *u = hex_to_num(*str) << 4;
        ++str;
        if (0 == *str)
            return -1;
        *u |= hex_to_num(*str);
        ++str;
        ++u;
    }
    return u == end ? 0 : -1;
}

