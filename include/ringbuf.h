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
