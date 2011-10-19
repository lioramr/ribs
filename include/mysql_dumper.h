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
