#ifndef _EPOLL_EVENT_ARRAY__H_
#define _EPOLL_EVENT_ARRAY__H_

#include "basic_epoll_event.h"
#include "ptr_array.h"
#include <sys/time.h>
#include <sys/resource.h>

struct epoll_event_array : public ptr_array<struct basic_epoll_event>
{
    template<typename F>
    void init(const F &f)
    {
        struct rlimit rlim;
        ::getrlimit(RLIMIT_NOFILE, &rlim);
        ptr_array<struct basic_epoll_event>::init((size_t)rlim.rlim_cur, f);
        int fd = 0;
        for (struct basic_epoll_event **ev = begin(), **evEnd = end(); ev != evEnd; ++ev, ++fd)
            (*ev)->fd = fd;
    }
};


#endif // _EPOLL_EVENT_ARRAY__H_
