#include "logger.h"

#include <sys/time.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "vmbuf.h"

const char *MC_INFO  = "INFO ";
const char *MC_ERROR = "ERROR";


inline vmbuf *log_buf()
{
    static __thread vmbuf *buf = NULL;
    if (NULL == buf)
        buf = new vmbuf;
    buf->init();
    return buf;
}

static vmbuf *begin_log_line(const char *msg_class)
{
    struct tm tm, *tmp;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    tmp = localtime_r(&tv.tv_sec, &tm);
    
    vmbuf *buf = log_buf();
    buf->reset();
    buf->strftime("%Y-%m-%d %H:%M:%S", tmp);
    pid_t tid = (pid_t) syscall (SYS_gettid);
    buf->sprintf(".%03d.%03d %d %s ", tv.tv_usec / 1000, tv.tv_usec % 1000, tid, msg_class);
    return buf;
}

inline void end_log_line(int fd, vmbuf *buf)
{
    *buf->wloc() = '\n';
    buf->wseek(1);
    buf->write(fd);
}


/* static */
int logger::create_log_file(const char *logfile)
{
    int fd = -1;
    if (NULL != logfile)
    {
        if (logfile[0] == '|')
        {
            // popen
            ++logfile;
            FILE *fp = popen(logfile, "w");
            if (NULL != fp)
                fd = fileno(fp);
        } else
        {
            // open
            fd = open(logfile, O_CREAT | O_WRONLY | O_APPEND, 0644);
        }
    }
    return fd;
}

/* static */
void logger::log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vlog(STDOUT_FILENO, format, MC_INFO, ap);
    va_end(ap);
}

/* static */
void logger::log_at(const char *filename, unsigned int linenum, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vlog_at(STDOUT_FILENO, filename, linenum, format, MC_INFO, ap);
    va_end(ap);
}

/* static */
void logger::error(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vlog(STDERR_FILENO, format, MC_ERROR, ap);
    va_end(ap);
}

/* static */
void logger::error_at(const char *filename, unsigned int linenum, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vlog_at(STDERR_FILENO, filename, linenum, format, MC_ERROR, ap);
    va_end(ap);
}

/* static */
void logger::perror(const char *format, ...)
{
    vmbuf *buf = begin_log_line(MC_ERROR);
    va_list ap;
    va_start(ap, format);

    char tmp[512];
    buf->vsprintf(format, ap);
    buf->sprintf(" (%s)", strerror_r(errno, tmp, 512));
    va_end(ap);
    end_log_line(STDERR_FILENO, buf);
}

/* static */
void logger::perror_at(const char *filename, unsigned int linenum, const char *format, ...)
{
    vmbuf *buf = begin_log_line(MC_ERROR);
    va_list ap;
    va_start(ap, format);

    buf->sprintf("[%s:%u]: ", filename, linenum);
    buf->vsprintf(format, ap);
    char tmp[512];
    buf->sprintf(" (%s)", strerror_r(errno, tmp, 512));
    va_end(ap);
    end_log_line(STDERR_FILENO, buf);
}

/* static */
void logger::vlog(int fd, const char *format, const char *msg_class, va_list ap)
{
    vmbuf *buf = begin_log_line(msg_class);
    buf->vsprintf(format, ap);
    end_log_line(fd, buf);
}

/* static */
void logger::vlog_at(int fd, const char *filename, unsigned int linenum, const char *format, const char *msg_class, va_list ap)
{
    vmbuf *buf = begin_log_line(msg_class);
    buf->sprintf("[%s:%u]: ", filename, linenum);
    buf->vsprintf(format, ap);
    end_log_line(fd, buf);
}

