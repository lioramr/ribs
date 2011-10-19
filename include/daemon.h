#ifndef _DAEMON__H_
#define _DAEMON__H_

#include <unistd.h>

struct daemon
{
    static int start(const char *cwd, const char *pidfile = NULL, const char *logfile = NULL);
    static void finish();
};

#endif // _DAEMON__H_
