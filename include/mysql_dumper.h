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
#ifndef _MYSQL_DUMPER__H_
#define _MYSQL_DUMPER__H_

#include <unistd.h>
#include "hashtable_vect.h"
#include "dump_common.h"

#define MYSQLDUMPER_UNSIGNED (0x8000)
#define MYSQLDUMPER_NULL_TERMINATED_STR (0x10000)

struct mysql_dumper
{
    static int dump(const char *dir,
                    const char *query,
                    size_t query_len,
                    struct db_login_info *login_info,
                    const char *db,
                    hashtable_vect<int> *types,
                    bool compress = true);
};

#endif // _MYSQL_DUMPER__H_
