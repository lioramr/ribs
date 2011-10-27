#ifndef _VM_BUF__H_
#define _VM_BUF__H_

#include <sys/mman.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "vmstorage.h"

template<typename S>
struct vmbuf_common
{
    S storage;
    
    vmbuf_common() : storage(), read_loc(0), write_loc(0) { unsigned int *cnt = allocated(); __sync_add_and_fetch(cnt, 1); }
    ~vmbuf_common() { free(); unsigned int *cnt = allocated(); __sync_sub_and_fetch(cnt, 1); }

    void detach() { storage.detach(); }
    
    void reset();
    int free();
    int free_most();
    
    int resize_by(size_t by);
    int resize_to(size_t new_capacity);
    int resize_if_full();
    int resize_if_less(size_t desired_size);
    int resize_no_check(size_t n);
    
    size_t alloc(size_t n);
    size_t alloczero(size_t n);

    template<typename T>
    T *alloc();

    template<typename T>
    int copy(const T &v);

    template<typename T>
    size_t num_elements();
    
    char *data() const { return storage.buf; }
    char *data(size_t loc) const { return storage.buf + loc; }

    size_t wavail() { return storage.capacity - write_loc; }
    size_t ravail() { return write_loc - read_loc; }

    char *wloc() const { return storage.buf + write_loc; }
    char *rloc() const { return storage.buf + read_loc; }

    size_t rlocpos() { return read_loc; }
    size_t wlocpos() { return write_loc; }
    void rollback(size_t ofs) { write_loc = ofs; }

    size_t capacity() { return storage.capacity; }

    void unsafe_wseek(size_t by) { write_loc += by; }
    int wseek(size_t by) { unsafe_wseek(by); return resize_if_full(); }
    void rseek(size_t by) { read_loc += by; }
    void rewind() { read_loc = 0; }
    
    vmbuf_common &sprintf(const char *format, ...);
    vmbuf_common &vsprintf(const char *format, va_list ap);
    vmbuf_common &strcpy(const char *src);

    vmbuf_common &remove_last_if(char c);

    int read(int fd);
    int write(int fd);

    int memcpy(const void *src, size_t n);
    int memcpy(size_t offset, size_t n);
    void memset(int c, size_t n) { ::memset(this->data(), c, n); }

    vmbuf_common &strftime(const char *format, const struct tm *tm);

    size_t read_loc;
    size_t write_loc;

    static unsigned int *allocated() { static unsigned int cnt = 0; return &cnt; }
};


struct vmbuf : vmbuf_common<vmstorage_mem>
{
    int init(size_t initial_size = vmpage::PAGESIZE)
    {
        if (0 > storage.init(initial_size))
            return -1;
        reset();
        return 0;
    }
};

struct vmfile : vmbuf_common<vmstorage_file>
{
    int create(const char *filename, size_t initial_size = vmpage::PAGESIZE)
    {
        if (0 > storage.create(filename, initial_size))
            return -1;
        reset();
        return 0;
    }

    int create(int fd, size_t initial_size = vmpage::PAGESIZE)
    {
        if (0 > storage.create(fd, initial_size))
            return -1;
        reset();
        return 0;
    }

    int create_tmp(size_t initial_size = vmpage::PAGESIZE)
    {
        if (0 > storage.create_tmp(initial_size))
            return -1;
        reset();
        return 0;
    }

    int load(const char *filename, int mmap_flags, int vmstorage_flags)
    {
        if (0 > storage.load(filename, mmap_flags))
            return -1;
        
        read_loc = 0;
        write_loc = storage.capacity;
        if (vmstorage_flags & VMSTORAGE_RO)
        	storage.close(); // can close the file after mmap
        return 0;
    }

    int finalize()
    {
        return storage.truncate(write_loc);
    }
};


/////////////////////
/// inline
/////////////////////

template<typename S>
inline void vmbuf_common<S>::reset()
{
    read_loc = write_loc = 0;
}

template<typename S>
inline int vmbuf_common<S>::free()
{
    reset();
    return storage.free();
}

template<typename S>
inline int vmbuf_common<S>::free_most()
{
    reset();
    return storage.free_most();
}

template<typename S>
inline int vmbuf_common<S>::resize_by(size_t by)
{
     return resize_to(storage.capacity + by);
}

template<typename S>
inline int vmbuf_common<S>::resize_to(size_t new_capacity)
{
    return storage.resize_to(new_capacity);
}

template<typename S>
inline int vmbuf_common<S>::resize_if_full()
{
    if (write_loc == storage.capacity)
        return resize_to(storage.capacity << 1);
    return 0;
}

template<typename S>
inline int vmbuf_common<S>::resize_if_less(size_t desired_size)
{
    size_t wa = wavail();
    if (desired_size <= wa)
        return 0;
    return resize_no_check(desired_size);
}

template<typename S>
inline int vmbuf_common<S>::resize_no_check(size_t n)
{
    size_t new_capacity = storage.capacity;
    do
    {
        new_capacity <<= 1;
    } while (new_capacity - write_loc <= n);
    return resize_to(new_capacity);
}

template<typename S>
inline size_t vmbuf_common<S>::alloc(size_t n)
{
    write_loc += 7; // align
    write_loc &= ~7;
    size_t loc = write_loc;
    if (0 > resize_if_less(n) || 0 > wseek(n))
        return -1;
    return loc;
}

template<typename S>
inline size_t vmbuf_common<S>::alloczero(size_t n)
{
    size_t loc = alloc(n);
    ::memset(data(loc), 0, n);
    return loc;
}


template<typename S>
template<typename T>
inline T *vmbuf_common<S>::alloc()
{
    size_t loc = write_loc;
    if (0 > resize_if_less(sizeof(T)))
        return NULL;
    wseek(sizeof(T));
    T *t = (T *)data(loc);
    return t;
}

template<typename S>
template<typename T>
inline int vmbuf_common<S>::copy(const T &v)
{
    if (0 > resize_if_less(sizeof(T)))
        return -1;
    *(T *)wloc() = v;
    wseek(sizeof(T));
    return 0;
}

template<typename S>
template<typename T>
inline size_t vmbuf_common<S>::num_elements()
{
    return wlocpos() / sizeof(T);
}

template<typename S>
inline vmbuf_common<S> &vmbuf_common<S>::sprintf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsprintf(format, ap);
    va_end(ap);
    return *this;
}

template<typename S>
inline vmbuf_common<S> &vmbuf_common<S>::vsprintf(const char *format, va_list ap)
{
    size_t n;
    for (;;)
    {
        size_t wav = wavail();
        va_list apc;
        va_copy(apc, ap);
        n = vsnprintf(wloc(), wav, format, apc);
        va_end(apc);
        if (n < wav)
            break;
        // not enough space, resize
        if (0 > resize_no_check(n))
            return *this;
    }
    wseek(n);
    return *this;
}

template<typename S>
inline vmbuf_common<S> &vmbuf_common<S>::strcpy(const char *src)
{
    do
    {
        char *w = wloc();
        char *dst = w;
        char *last = w + wavail();
        for (; dst < last && *src; *dst++ = *src++); // copy until end of string or end of buf
        if (0 > wseek(dst - w)) // this will trigger resize if needed
            return *this; // error
    } while (*src);
    *(wloc()) = 0; // trailing \0
    return *this;
}

template<typename S>
inline vmbuf_common<S> &vmbuf_common<S>::remove_last_if(char c)
{
    char *loc = wloc();
    --loc;
    if (write_loc > 0 && *loc == c)
    {
        --write_loc;
        *loc = 0;
    }
    return *this;
}

template<typename S>
inline int vmbuf_common<S>::read(int fd)
{
    ssize_t res;
    while (0 < (res = ::read(fd, wloc(), wavail())))
    {
        if (0 > wseek(res))
            return -1;
    }
    if (res < 0)
        return (EAGAIN == errno ? 1 : -1);
    return 0; // remote side closed connection
}


template<typename S>
inline int vmbuf_common<S>::write(int fd)
{
    ssize_t res;
    size_t rav;
    while ((rav = ravail()) > 0)
    {
        res = ::write(fd, rloc(), ravail());
        if (res < 0)
            return (EAGAIN == errno ? 0 : -1);
        else if (res > 0)
            rseek(res);
        else
            return errno = ENODATA, -1; // error, can not be zero
    }
    return 1; // reached the end
}

template<typename S>
inline int vmbuf_common<S>::memcpy(const void *src, size_t n)
{
    resize_if_less(n);
    ::memcpy(wloc(), src, n);
    return wseek(n);
}

template<typename S>
inline int vmbuf_common<S>::memcpy(size_t offset, size_t n)
{
    if ((offset+n) > write_loc) return 0;
    resize_if_less(n);
    ::memcpy(wloc(), data(offset), n);
    return wseek(n);
}


template<typename S>
inline vmbuf_common<S> &vmbuf_common<S>::strftime(const char *format, const struct tm *tm)
{
    size_t n;
    for (;;)
    {
        size_t wav = wavail();
        n = ::strftime(wloc(), wav, format, tm);
        if (n > 0)
            break;

        // not enough space, resize
        if (0 > resize_by(vmpage::PAGESIZE))
            return *this;
    }
    wseek(n);
    return *this;
}


#endif // _VM_BUF__H_
