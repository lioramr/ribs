#ifndef _SIGNAL_HANDLER__H_
#define _SIGNAL_HANDLER__H_

#include <sys/signalfd.h>
#include <signal.h>

struct signal_handler : basic_epoll_event
{
    int init(void (*cb)(), int sig)
    {
        callback = cb;
        this->method.set(&signal_handler::on_signal);
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, sig);
        
        this->fd = signalfd(-1, &set, SFD_NONBLOCK);
        if (0 > this->fd)
        {
            perror("signalfd");
            return -1;
        }
        return 0;
    }

    void init_per_thread()
    {
        epoll::add_multi(this);
    }

    struct basic_epoll_event *on_signal()
    {
        struct signalfd_siginfo siginfo;
        ssize_t n;
        do
        {
            n = ::read(this->fd, &siginfo, sizeof(siginfo));
            if (0 > n)
            {
                if (EAGAIN == errno)
                    return NULL;
                perror("signal read");
            } else
            {
                callback();
            }
        } while (n > 0);
        return NULL;
    }
    
    void (*callback)();
};

#endif // _SIGNALFD__H_
