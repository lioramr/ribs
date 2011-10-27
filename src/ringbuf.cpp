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

int ringbuf::init(size_t size)
{
    char tmp[] = "/dev/shm/ringbuffer_mem_XXXXXX";
    int res = -1;
    char *mem = NULL;

    int fd = mkstemp(tmp);
    if (0 > fd)
    {
        perror("mkstemp");
        return -1;
    }
    if (0 > unlink(tmp))
    {
        perror("unlink");
        goto ringbuf_error;
    }

    if (size < PAGESIZE)
        size = PAGESIZE;
    size += PAGEMASK;
    size &= ~PAGEMASK;

    if (0 > ftruncate(fd, size))
    {
        perror("ftruncate");
        goto ringbuf_error;
    }
    // mmap 2x the size for getting 2x address space, physical will be 1x
    mem = (char *)mmap(NULL, size << 1, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == mem)
    {
        perror("mmap");
        mem = NULL;
        goto ringbuf_error;
    }
    buf = mem;

    // overlap 2 addresses with same physical memory
    // region 1 (0..size-1)
    mem = (char *)mmap(buf, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
    if (mem != buf)
        goto ringbuf_error;

    // region 2 (size..size*2-1)
    mem = (char *)mmap(buf + size, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
    if (mem != buf + size)
        goto ringbuf_error;

    capacity = size;
    avail = capacity;
    reset();
    
    res = 0;
 ringbuf_error:
    if (0 > ::close(fd))
        perror("close");
    return res;
}

int ringbuf::free()
{
    return 0;
}

