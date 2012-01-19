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
#ifndef _RUUID__H_
#define _RUUID__H_

#include <stdint.h>
#include <vmbuf.h>

struct uuid
{
    uint8_t bytes[16];
};

struct uuid_str
{
    char str[33];
};

int uuid_init();
int uuid_init_thread();
int uuid_generate(struct uuid *bytes);
void uuid_2_str(struct uuid *uuid, uuid_str *uuid_str);
void uuid_2_vmbuf(struct uuid *uuid, vmbuf *buf);
int uuid_parse(struct uuid *uuid, const char *str);

#endif // _RUUID__H_
