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
#ifndef _DUMP_COMMON__H_
#define _DUMP_COMMON__H_

#include "hashtable_vect.h"
#include "logger.h"
#include "ds_link_creator.h"
#include "VarFieldReader.h"
#include "VarFieldWriter.h"
#include "index.h"

int mkdir_exist(const char *dir);
int prepare_dumper(const char *base_dir, const char *db, const char *table, vmbuf *target_dir);
void mask_db_pass(char *str);
int dump_data(const char * target_dir, const char *query, size_t query_len, const char *conn_str, const char *database, hashtable_vect<int> *types);
template<typename L, typename R>
int generate_ds_link(const char *rootdir, const char *dbname, const char *src, const char *field, const char *dst);
int generate_ds_link_one_to_many(const char *rootdir, const char *dbname, const char *src, const char *field, const char *dst);
int load_field(const char *base_dir, const char *table, const char *field, ds_field_base *res);
int load_var_field(const char *base_dir, const char *table, const char *field, VarFieldReader *res);
int write_field(const char *base_dir, const char *table, const char *field, ds_field_base_write *res);
int write_var_field(const char *base_dir, const char *table, const char *field, VarFieldWriter *res);

int generate_data_version(const char *target_dir, const char *compile_revision);

template<typename T>
int load_index(const char *base_dir, const char *table, const char *field, index_container<T> *res)
{
    vmbuf tmp;
    tmp.init();
    tmp.sprintf("%s/%s/%s", base_dir, table, field);
    return res->init(tmp.data());
}

struct db_login_info
{
    const char *user;
    const char *pass;
    const char *host;
    uint16_t port;
};

int parse_db_conn_str(char *str, struct db_login_info *info);

/*
 * inline
 */

template<typename L, typename R>
inline int generate_ds_link(const char *basedir, const char *dbname, const char *src, const char *field, const char *dst)
{
    logger::log("%s/%s ==> %s", src, field, dst);
    char buf_ldir[8192];
    char buf_rfile[8192];
    sprintf(buf_ldir, "%s/%s/%s", basedir, dbname, src);
    sprintf(buf_rfile, "%s/%s/%s", basedir, dbname, dst);
    if (0 > ds_link_creator<L, R>::generate_one_to_one(buf_ldir, field, buf_rfile))
    {
        logger::log("links failed");
        return -1;
    }
    return 0;
}

    

#endif // _DUMP_COMMON__H_

    
