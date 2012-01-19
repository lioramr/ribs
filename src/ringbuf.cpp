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
#include "ringbuf.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int ringbuf::inittmp(size_t size)
{
    char tmp[] = "/dev/shm/ringbuffer_mem_XXXXXX";
    int fd = mkstemp(tmp);
    if (0 > fd)
    {
        perror("mkstemp");
        return -1;
    }
    if (0 > unlink(tmp))
    {
        perror("unlink");
        close(fd);
        return -1;
    }
    if (0 > initfd(fd, size))
    {
        ::close(fd);
        return -1;
    }
    reset();
    return 0;
}

int ringbuf::initfd(int fd, size_t size)
{
    char *mem = NULL;

    if (size < PAGESIZE)
        size = PAGESIZE;
    size += PAGEMASK;
    size &= ~PAGEMASK;

    struct stat st;
    if (0 > fstat(fd, &st))
    {
        perror("fstat");
        return -1;
    }
    // reserve extra page for misc data
    if (0 > ftruncate(fd, size + PAGESIZE))
    {
        perror("ftruncate");
        return -1;
    }
    // mmap 2x the size for getting 2x address space, physical will be 1x
    mem = (char *)mmap(NULL, (size << 1) + PAGESIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == mem)
    {
        perror("mmap");
        return -1;
    }
    buf = mem;

    mem = (char *)mmap(buf, 4096, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
    if (mem != buf)
        goto ringbuf_mmap_err;

    this->header = (struct header_data *)mem;

    if (st.st_size != (off_t)size + PAGESIZE)
        reset(); // reset so rloc & wloc are valid

    buf += PAGESIZE;

    // overlap 2 addresses with same physical memory
    // region 1 (0..size-1)
    mem = (char *)mmap(buf, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, PAGESIZE);
    if (mem != buf)
        goto ringbuf_mmap_err;

    // region 2 (size..size*2-1)
    mem = (char *)mmap(buf + size, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, PAGESIZE);
    if (mem != buf + size)
        goto ringbuf_mmap_err;

    capacity = size;
    avail = size - (header->write_loc - header->read_loc);

    return 0;
 ringbuf_mmap_err:
    munmap(buf - PAGESIZE, (size << 1) + PAGESIZE);
    return -1;
}

int ringbuf::free()
{
    if (buf != NULL && 0 > munmap(buf - PAGESIZE, (capacity << 1) + PAGESIZE))
        return perror("ringbuf::free, munmap"), -1;
    return 0;
}

