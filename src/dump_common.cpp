#include "dump_common.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include "logger.h"
#include "vmbuf.h"
#include "ds.h"
#include "mysql_dumper.h"


int mkdir_exist(const char *dir)
{
    if (0 > mkdir(dir, 0755) && (errno != EEXIST)) 
    {
        perror("mkdir");
        return -1;
    }
    return 0;
}

int prepare_dumper(const char *base_dir, const char *db, const char *table, vmbuf *target_dir)
{
    logger::log("preparing to dump %s.%s", db, table);
    target_dir->init();
    target_dir->sprintf("%s/%s", base_dir, db);
    if (0 > mkdir_exist(target_dir->data()))
        return -1;
    target_dir->sprintf("/%s", table);
    if (0 > mkdir_exist(target_dir->data()))
        return -1;
    return 0;
}

void mask_db_pass(char *str)
{
    char *p = strchrnul(str, '@');
    if (*p)
    {
        char *p1 = strchrnul(str, '/');
        if (p1 < p)
            memset(p1 + 1, '*', p - p1 - 1);
    }
}

int dump_data(const char * target_dir, const char *query, size_t query_len, const char *conn_str, const char *database, hashtable_vect<int> *types)
{
    logger::log("query: [%s]", query);
    logger::log("target dir: [%s]", target_dir);

    size_t n = strlen(conn_str);
    char buf[n + 1];
    strcpy(buf, conn_str);

    struct db_login_info login_info;
    if (0 > parse_db_conn_str(buf, &login_info))
    {
        logger::log("failed to parse connection string: [%s]", conn_str);
        return -1;
    }

    logger::log("DB: [%s@%s:%d][%s]", login_info.user, login_info.host, login_info.port, database);
       
    if (0 > mysql_dumper::dump(target_dir, query, query_len, &login_info, database, types))
    {
        return -1;
    }
    return 0;
}

int generate_ds_link_one_to_many(const char *basedir, const char *dbname, const char *src, const char *field, const char *dst)
{
    logger::log("%s/%s ==> %s", src, field, dst);
    char buf_ldir[8192];
    char buf_rfile[8192];
    sprintf(buf_ldir, "%s/%s/%s", basedir, dbname, src);
    sprintf(buf_rfile, "%s/%s/%s", basedir, dbname, dst);
    if (0 > ds_link_creator<int32_t, uint32_t>::generate_one_to_many(buf_ldir, field, buf_rfile))
    {
        logger::log("links failed");
        return -1;
    }
    return 0;
}

int parse_db_conn_str(char *str, struct db_login_info *info)
{
    memset(info, 0, sizeof(struct db_login_info));
    info->user = cuserid(NULL);
    info->pass = "";
    info->host = "";
    info->port = 0;
    
    char *p = strchrnul(str, '@');
    if (*p)
    {
        *p++ = 0;
        info->user = str;
        info->host = p;

        p = strchrnul(p, ':');
        if (*p)
        {
            *p++ = 0;
            info->port = atoi(p);
        }
        
        p = strchrnul(str, '/');
        if (*p)
        {
            *p++ = 0;
            info->pass = p;
        }
    } else
        return -1;
    return 0;
}

int load_field(const char *base_dir, const char *table, const char *field, ds_field_base *res)
{
    vmbuf tmp;
    tmp.init();
    tmp.sprintf("%s/%s/%s", base_dir, table, field);
    return res->init(tmp.data());
}

int load_var_field(const char *base_dir, const char *table, const char *field, VarFieldReader *res)
{
    vmbuf tmp;
    tmp.init();
    tmp.sprintf("%s/%s/%s", base_dir, table, field);
    return res->init(tmp.data());
}

int write_field(const char *base_dir, const char *table, const char *field, ds_field_base_write *res)
{
    vmbuf tmp;
    tmp.init();
    tmp.sprintf("%s/%s/%s", base_dir, table, field);
    return res->init(tmp.data());
}

int write_var_field(const char *base_dir, const char *table, const char *field, VarFieldWriter *res)
{
    vmbuf tmp;
    tmp.init();
    tmp.sprintf("%s/%s/%s", base_dir, table, field);
    return res->init(tmp.data());
}

int generate_data_version(const char *target, const char *compile_revision)
{
    vmbuf tmp;
    tmp.init();
    tmp.sprintf("%s/version", target);
    vmfile data_version;
    if (0 > data_version.create(tmp.data()))
        return -1;
    struct tm tm;
    time_t now = time(NULL);
    localtime_r(&now, &tm);
    data_version.strftime("%Y%m%d%H%M%S", &tm);
    data_version.sprintf("\n%s\n", compile_revision);
    if (0 > data_version.finalize())
        return -1;
    data_version.free();
    return 0;

}
