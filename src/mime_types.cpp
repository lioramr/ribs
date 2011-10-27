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
#include "mime_types.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sstr.h"

SSTR(MIME_TYPES, "/etc/mime.types");
SSTR(MIME_DELIMS, "\t ");
SSTR(DEFAULT_MIME_TYPE, "text/plain");

mime_types::mime_types()
{
    htMimes.init(2048);
}

mime_types::~mime_types()
{
}

/* static */
mime_types *mime_types::instance()
{
    static struct mime_types mime_types;
    return &mime_types;
}

void mime_types::load()
{
    int fd = open(MIME_TYPES, O_RDONLY);
    struct stat st;
    if (0 > fstat(fd, &st))
    {
        perror("mime_types fstat");
        exit(EXIT_FAILURE);
    }
    void *mem = mmap(NULL, st.st_size + 1, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    char *content = (char *)mem;
    char *contentEnd = content + st.st_size;

    while (content < contentEnd)
    {
        // find end of line
        char *line = content, *p = line;
        for (; p < contentEnd && *p != '\n'; ++p);
        *p = 0; // mmap'd one extra byte to avoid checks
        content = p + 1;
        
        p = strchrnul(line, '#');
        *p = 0; // truncate line after comment

        p = line;
        char *saveptr;
        char *mime = strtok_r(p, MIME_DELIMS, &saveptr);
        if (!mime)
            continue;
        //printf("mime: [%s]\n", mime);
        char *ext;
        while (NULL != (ext = strtok_r(NULL, MIME_DELIMS, &saveptr)))
        {
            for (p = ext; *p; *p = tolower(*p), ++p);
            //printf("    ext: [%s]\n", ext);
            htMimes.insert(ext, strlen(ext), mime, strlen(mime));
        }
    }
    munmap(mem, st.st_size + 1);
    ::close(fd);
}

const char *mime_types::type(const char *ext)
{
    const char *res = DEFAULT_MIME_TYPE;
    size_t n = strlen(ext);
    char tmpext[n];
    char *p = tmpext;
    for (; *ext; ++ext)
        *p++ = tolower(*ext);
    // tmpext doesn't have to be \0 terminated
    size_t ofs = htMimes.lookup(tmpext, n);
    if (htMimes.is_found(ofs))
        return htMimes.get_val(ofs);
    return res;
}

const char *mime_types::mime_type(const char *file_name)
{
    const char *p = strrchr(file_name, '.');
    if (p)
        return type(p + 1);
    return DEFAULT_MIME_TYPE;
}
