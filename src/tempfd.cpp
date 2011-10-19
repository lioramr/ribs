#include "tempfd.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

namespace tempfd
{
    int create()
    {
        char tmp[] = "/dev/shm/tmpfd_XXXXXX";
        int fd = mkstemp(tmp);
        if (0 > fd)
        {
            perror("mkstemp");
            return -1;
        }
        if (0 > unlink(tmp))
        {
            perror("unlink");
            ::close(fd);
            return -1;
        }
        return fd;
    }
}
