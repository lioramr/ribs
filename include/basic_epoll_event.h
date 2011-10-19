#ifndef _BASIC_EPOLL_EVENT__H_
#define _BASIC_EPOLL_EVENT__H_

#include <sys/epoll.h>
#include <unistd.h>

struct basic_epoll_event;

struct basic_epoll_event_method_0args
{
    typedef struct basic_epoll_event *(basic_epoll_event::* method_t)();

    template<typename T>
    void set(basic_epoll_event *(T::* m)())
    {
        method = (method_t)m;
    }

    struct basic_epoll_event *invoke(struct basic_epoll_event *e) __attribute__ ((warn_unused_result))
    {
        return (e->*method)();
    }

    method_t method;
};


struct basic_epoll_event_callback_method_0arg
{
    typedef struct basic_epoll_event *(basic_epoll_event::* method_t)();

    basic_epoll_event_callback_method_0arg() : event(NULL) {}

    template<typename U>
    void set(struct basic_epoll_event *e, struct basic_epoll_event *(U::* m)())
    {
        event = e;
        method = (method_t)m;
    }

    struct basic_epoll_event *invoke()  __attribute__ ((warn_unused_result))
    {
        return (event->*method)();
    }
    
    struct basic_epoll_event *event;
    method_t method;
};


template<typename T=basic_epoll_event *>
struct basic_epoll_event_callback_method_1arg
{
    typedef struct basic_epoll_event *(basic_epoll_event::* method_t)(T);

    basic_epoll_event_callback_method_1arg() : event(NULL) {}

    template<typename U>
    void set(basic_epoll_event *e, basic_epoll_event *(U::* m)(T))
    {
        event = e;
        method = (method_t)m;
    }

    struct basic_epoll_event *invoke(T t)  __attribute__ ((warn_unused_result))
    {
        return (event->*method)(t);
    }
    struct basic_epoll_event *event;
    method_t method;
};

struct basic_epoll_event
{
    basic_epoll_event() : fd(-1) {}
    struct basic_epoll_event *invoke() { return method.invoke(this); }

    struct basic_epoll_event_method_0args method;
    struct basic_epoll_event *timeout_chain_next;
    struct basic_epoll_event *timeout_chain_prev;
    struct timeval last_event_ts;
    int fd;
};

#endif // _BASIC_EPOLL_EVENT__H_
