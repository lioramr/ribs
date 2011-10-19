#ifndef _VM_STORAGE__H_
#define _VM_STORAGE__H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tempfd.h"


struct vmpage
{
    enum
    {
        PAGEMASK = 4095,
        PAGESIZE
    };

    inline static off_t align(off_t off)
    {
        off += PAGEMASK;
        off &= ~PAGEMASK;
        return off;
    }
};

struct vmstorage_mem
{
    vmstorage_mem() : buf(NULL), capacity(0) {}
    void detach() { buf = NULL; capacity = 0; }
    int init(size_t initial_size)
    {
        if (NULL == buf)
        {
            initial_size = vmpage::align(initial_size);
            buf = (char *)mmap(NULL, initial_size, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            if (MAP_FAILED == buf)
            {
                perror("mmap, vmstorage_mem::init");
                buf = NULL;
                return -1;
            }
            capacity = initial_size;
        } else if (capacity < initial_size)
        {
            return resize_to(initial_size);
        }
        return 0;
    }

    int free()
    {
        if (NULL != buf && 0 > munmap(buf, capacity))
        {
            perror("munmap vmstorage_mem::free");
            return -1;
        }
        buf = NULL;
        capacity = 0;
        return 0;
    }

    int free_most()
    {
        if (NULL != buf && capacity > vmpage::PAGESIZE)
        {
            if (0 > munmap(buf + vmpage::PAGESIZE, capacity - vmpage::PAGESIZE))
            {
                perror("munmap vmbuf_common<S>::free_most");
                return -1;
            }
            capacity = vmpage::PAGESIZE;
        }
        return 0;
    }
    
    int resize_to(size_t new_capacity)
    {
        new_capacity = vmpage::align(new_capacity);
        char *newaddr = (char *)mremap(buf, capacity, new_capacity, MREMAP_MAYMOVE);
        if ((void *)-1 == newaddr)
        {
            perror("mremap vmstorage_mem::resize_to");
            return -1;
        }
        // success
        buf = newaddr;
        capacity = new_capacity;
        return 0;
    }
    
    char *buf;
    size_t capacity;
};


struct vmstorage_file
{
    vmstorage_file() : buf(NULL), capacity(0), fd(-1) {}
    void detach() { buf = NULL; capacity = 0; fd = -1; }
    
    int create(int fd, size_t initial_size)
    {
        this->fd = fd;
        initial_size = vmpage::align(initial_size);
        if (0 > ftruncate(fd, initial_size))
        {
            perror("ftruncate");
            return -1;
        }
        buf = (char *)mmap(NULL, initial_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
        if (MAP_FAILED == buf)
        {
            perror("mmap, vmstorage_file::create");
            buf = NULL;
            return -1;
        }
        capacity = initial_size;
        return 0;
    }

    int create_tmp(size_t initial_size)
    {
        int tfd = tempfd::create();
        if (0 > tfd)
            return -1;
        return create(tfd, initial_size);
    }
    
    int create(const char *filename, size_t initial_size)
    {
        if (NULL == buf)
        {
            if (0 > unlink(filename) && errno != ENOENT)
                return perror(filename), -1;
            
            fd = open(filename, O_CREAT | O_RDWR, 0644);
            if (0 > fd)
                return perror(filename), -1;

            return create(fd, initial_size);
        } else if (capacity < initial_size)
        {
            return resize_to(initial_size);
        }
        return 0;
    }

    int load(const char *filename)
    {
        if (0 > this->free())
            return -1;
        
        fd = open(filename, O_RDWR);
        if (0 > fd)
        {
            perror(filename);
            return -1;
        }

        size_t len;
        struct stat st;
        if (0 > fstat(fd, &st))
        {
            perror(filename);
            return -1;
        }
        len = st.st_size;

        len = vmpage::align(len);
        
        buf = (char *)mmap(NULL, len, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
        if (MAP_FAILED == buf)
        {
            perror("mmap, vmstorage_file::load");
            buf = NULL;
            return -1;
        }
        capacity = len;
        return 0;
    }

    void close()
    {
        if (fd >= 0)
        {
            if (0 > ::close(fd))
                perror("close");
            fd = -1;
        }
    }
    
    int free()
    {
        this->close();
        if (NULL != buf && 0 > munmap(buf, capacity))
        {
            perror("munmap vmstorage_file::free");
            return -1;
        }
        buf = NULL;
        capacity = 0;
        return 0;
    }

    int free_most()
    {
        if (NULL != buf && capacity > vmpage::PAGESIZE)
        {
            if (0 > munmap(buf + vmpage::PAGESIZE, capacity - vmpage::PAGESIZE))
            {
                perror("munmap vmbuf_file::free_most");
                return -1;
            }
            capacity = vmpage::PAGESIZE;
            if (0 > ftruncate(fd, capacity))
            {
                perror("ftruncate, free_most");
                return -1;
            }
        }
        return 0;
    }
    
    int resize_to(size_t new_capacity)
    {
        new_capacity = vmpage::align(new_capacity);
        if (0 > ftruncate(fd, new_capacity))
        {
            perror("ftruncate");
            return -1;
        }
        char *newaddr = (char *)mremap(buf, capacity, new_capacity, MREMAP_MAYMOVE);
        if ((void *)-1 == newaddr)
        {
            perror("mremap vmstorage_file::resize_to");
            return -1;
        }
        // success
        buf = newaddr;
        capacity = new_capacity;
        return 0;
    }

    int truncate(off_t len)
    {
        if (0 > ftruncate(fd, len))
        {
            perror("ftruncate");
            return -1;
        }
        return 0;
    }
    
    char *buf;
    size_t capacity;
    int fd;
};


#endif // _VM_STORAGE__H_
