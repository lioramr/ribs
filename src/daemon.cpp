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
#include "daemon.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include "logger.h"

static char *stored_pidfile = NULL;

inline static void init_stdio(int fd)
{
    if (fd >= 0)
    {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
    } else
        perror("init_stdio");
}

inline static int create_pidfile(const char *pidfile)
{
    int fd = open(pidfile, O_WRONLY | O_CREAT /*| O_EXCL*/, 0644);
    if (0 > fd)
        LOGGER_PERROR("pidfile: %s", pidfile);
    return fd;
}

inline static void reset_fds()
{
    int fdnull = open("/dev/null", O_RDWR);
    dup2(fdnull, STDIN_FILENO);
    dup2(fdnull, STDOUT_FILENO);
    dup2(fdnull, STDERR_FILENO);
    ::close(fdnull);
}

inline static void init_logfile(const char *logfile)
{
    if (NULL != logfile)
    {
        if (logfile[0] == '|')
        {
            // popen
            ++logfile;
            FILE *fp = popen(logfile, "w");
            if (NULL != fp)
                init_stdio(fileno(fp));
            ::close(fileno(fp));
        } else
        {
            // open
            int fd = open(logfile, O_CREAT | O_WRONLY | O_APPEND, 0644);
            init_stdio(fd);
            ::close(fd);
        }
    } 
}

/* static */
int daemon::start(const char *cwd, const char *pidfile /* = NULL */, const char *logfile /* = NULL */)
{
    int fdpid = -1;
    if (pidfile)
    {
        stored_pidfile = strdup(pidfile);
        if (NULL == stored_pidfile)
            LOGGER_ERROR_STR_EXIT(EXIT_FAILURE, "daemon::start, strdup");
        
        if ((fdpid = create_pidfile(pidfile)) < 0)
            exit(EXIT_FAILURE);
    }
    
    pid_t pid = fork();
    if (pid < 0)
    {
        logger::perror("daemon::start, fork");
        exit(EXIT_FAILURE);
    }
        
    if (pid > 0)
    {
        logger::log("daemon started (pid=%d)", pid);
        exit(EXIT_SUCCESS);
    }
   
    umask(0);
    
    if (0 > setsid())
    {
        logger::perror("setsid");
        exit(EXIT_FAILURE);
    }

    if (NULL != cwd && 0 > chdir("/"))
    {
        logger::perror("chdir");
        exit(EXIT_FAILURE);
    }

    reset_fds();
    
    init_logfile(logfile);
    
    pid = getpid();
    if (fdpid >= 0)
    {
        dprintf(fdpid, "%d", pid);
        ::close(fdpid);
    }
    
    logger::log("child process started (pid=%d)", pid);
    return 0;
}

/* static */
void daemon::finish()
{
    if (NULL != stored_pidfile && 0 > unlink(stored_pidfile))
        logger::perror("unlink: [%s]", stored_pidfile);
    logger::log("exiting...");
    exit(EXIT_SUCCESS);
}
