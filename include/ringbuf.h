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
#ifndef _RING_BUF__H_
#define _RING_BUF__H_

#include <unistd.h>

struct ringbuf
{
    enum { PAGEMASK = 4095, PAGESIZE };
    ringbuf() : capacity(0), avail(0), read_loc(0), write_loc(0), buf(NULL) {}
    ~ringbuf() { free(); }
    
    int init(size_t size);
    int free();

    void reset() { read_loc = write_loc = 0; }

    char *wloc() { return buf + write_loc; }
    char *rloc() { return buf + read_loc; }

    void wseek(size_t by) { write_loc += by; avail -= by;}
    void rseek(size_t by)
    {
        avail += by;
        read_loc += by;
        if (read_loc >= capacity)
        {
            read_loc -= capacity;
            write_loc -= capacity;
        }
    }

    template<typename T>
    void write(T val)
    {
        if (avail < sizeof(T))
            rseek(sizeof(T));
        *(T *)wloc() = val;
        wseek(sizeof(T));
    }

    template<typename T>
    void read(T *val)
    {
        *val = *(T *)rloc();
        rseek(sizeof(T));
    }

    template<typename T>
    T read()
    {
        T t = *(T *)rloc();
        rseek(sizeof(T));
        return t;
    }

    template<typename T>
    void peek(T *val)
    {
        *val = *(T *)rloc();
    }

    template<typename T>
    void pop()
    {
        rseek(sizeof(T));
    }

    template<typename T>
    size_t num_elements()
    {
        return (write_loc - read_loc) / sizeof(T);
    }

    bool empty() { return read_loc == write_loc; }
    bool full() { return avail == 0; }
    
    size_t capacity;
    size_t avail;
    size_t read_loc;
    size_t write_loc;
    char *buf;
};

#endif // _RING_BUF__H_
