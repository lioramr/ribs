#ifndef _MMAP_FILE__H_
#define _MMAP_FILE__H_

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "logger.h"

struct mmap_file_write
{
    mmap_file_write() : fd (-1) {}
    ~mmap_file_write() { this->close(); }
    
    inline int init(const char *filename);
    inline ssize_t write(const void *buf, size_t count);
    inline int close();
    
    int get_fd() { return fd; }

    size_t get_wloc() const { return wloc; }

    int fd;
    size_t wloc;
};

struct mmap_file
{
    mmap_file() : mem(NULL), size(0) {}
    ~mmap_file() { this->close(); }
    
    inline int init(const char *filename, int prot = PROT_READ, int flags = MAP_SHARED);
    
    inline int close();

    size_t mem_size() { return size; }
    char *mem_begin() { return mem; }
    char *mem_end() { return mem + size; }

    char *mem;
    size_t size;
};

/*
 * inline
 */

/******************
* mmap_file_write *
*******************/
inline int mmap_file_write::init(const char *filename)
{
    wloc = 0;
    if (0 > unlink(filename) && errno != ENOENT)
        LOGGER_PERROR_STR(filename);
    fd = open(filename, O_CREAT | O_WRONLY, 0644);
    if (0 > fd)
        LOGGER_PERROR_STR(filename);
    return fd;
}

inline ssize_t mmap_file_write::write(const void *buf, size_t count)
{
    ssize_t res = ::write(fd, buf, count);
    if (res < 0)
        LOGGER_PERROR_STR("write");
    else
        wloc += res;
    return res;
}

inline int mmap_file_write::close()
{
    int res = ::close(fd);
    fd = -1;
    return res;
}

/************
* mmap_file *
*************/
inline int mmap_file::init(const char *filename, int prot /* = PROT_READ */, int flags /* = MAP_SHARED */)
{
    if (NULL != mem)
        this->close();
    int fd = open(filename, O_RDONLY);
    if (0 > fd)
        return LOGGER_PERROR_STR(filename), -1;
    struct stat st;
        
    if (0 > fstat(fd, &st))
        goto FixedFieldReader_init_error;

    size = st.st_size;
    mem = (char *)mmap(NULL, size, prot, flags, fd, 0);
    if (MAP_FAILED == mem)
    {
        mem = NULL;
        if (size != 0)
            goto FixedFieldReader_init_error; // something really bad happened
    }
    ::close(fd);
    return 0;
 FixedFieldReader_init_error:
    LOGGER_PERROR_STR(filename);
    ::close(fd);
    return -1;        
}

inline int mmap_file::close()
{
    if (NULL != mem && 0 > munmap(mem, size))
        return perror("munmap"), -1;
    
    mem = NULL;
    size = 0;
    return 0;
}

#endif // _MMAP_FILE__H_
