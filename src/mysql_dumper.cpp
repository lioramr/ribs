#include "mysql_dumper.h"
#include <stdio.h>
#include <errno.h>
#include <mysql/mysql.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include "vmbuf.h"
#include "ds.h"
#include "ds_type_mapper.h"
#include "mmap_file.h"
#include "VarFieldWriter.h"

#define ENUM_TO_STRING(e) \
    case e: \
    return (#e)

#define ENUM_2_STORAGE(e, t) \
    case e: \
    return (sizeof(t))

#define ENUM_2_STORAGE_STR(e, l) \
    case e:                      \
    return ((l)+1);

#define IS_UNSIGNED(f)  ((f) & UNSIGNED_FLAG)

#define MYSQL_TYPE_TO_DS(m, is_un, ts, tus)         \
    case m:                                         \
    return is_un ? ds_type_mapper::type_to_enum((tus *)NULL) : ds_type_mapper::type_to_enum((ts *)NULL);

int get_ds_type(int type, bool is_unsigned)
{
    switch(type)
    {
        MYSQL_TYPE_TO_DS(MYSQL_TYPE_TINY, is_unsigned, int8_t, uint8_t);
        MYSQL_TYPE_TO_DS(MYSQL_TYPE_SHORT, is_unsigned, int16_t, uint16_t);
        MYSQL_TYPE_TO_DS(MYSQL_TYPE_LONG, is_unsigned, int32_t, uint32_t);
        MYSQL_TYPE_TO_DS(MYSQL_TYPE_LONGLONG, is_unsigned, int64_t, uint64_t);
        MYSQL_TYPE_TO_DS(MYSQL_TYPE_FLOAT, is_unsigned, float, float);
        MYSQL_TYPE_TO_DS(MYSQL_TYPE_DOUBLE, is_unsigned, double, double);
        MYSQL_TYPE_TO_DS(MYSQL_TYPE_INT24, is_unsigned, int32_t, uint32_t);
        MYSQL_TYPE_TO_DS(MYSQL_TYPE_YEAR, is_unsigned, uint16_t, uint16_t);
    }
    return -1;
}

const char *get_type_name(int type)
{
    switch (type)
    {
        ENUM_TO_STRING(MYSQL_TYPE_DECIMAL);
        ENUM_TO_STRING(MYSQL_TYPE_TINY);
        ENUM_TO_STRING(MYSQL_TYPE_SHORT);
        ENUM_TO_STRING(MYSQL_TYPE_LONG);
        ENUM_TO_STRING(MYSQL_TYPE_FLOAT);
        ENUM_TO_STRING(MYSQL_TYPE_DOUBLE);
        ENUM_TO_STRING(MYSQL_TYPE_NULL);
        ENUM_TO_STRING(MYSQL_TYPE_TIMESTAMP);
        ENUM_TO_STRING(MYSQL_TYPE_LONGLONG);
        ENUM_TO_STRING(MYSQL_TYPE_INT24);
        ENUM_TO_STRING(MYSQL_TYPE_DATE);
        ENUM_TO_STRING(MYSQL_TYPE_TIME);
        ENUM_TO_STRING(MYSQL_TYPE_DATETIME);
        ENUM_TO_STRING(MYSQL_TYPE_YEAR);
        ENUM_TO_STRING(MYSQL_TYPE_NEWDATE);
        ENUM_TO_STRING(MYSQL_TYPE_VARCHAR);
        ENUM_TO_STRING(MYSQL_TYPE_BIT);
        ENUM_TO_STRING(MYSQL_TYPE_NEWDECIMAL);
        ENUM_TO_STRING(MYSQL_TYPE_ENUM);
        ENUM_TO_STRING(MYSQL_TYPE_SET);
        ENUM_TO_STRING(MYSQL_TYPE_TINY_BLOB);
        ENUM_TO_STRING(MYSQL_TYPE_MEDIUM_BLOB);
        ENUM_TO_STRING(MYSQL_TYPE_LONG_BLOB);
        ENUM_TO_STRING(MYSQL_TYPE_BLOB);
        ENUM_TO_STRING(MYSQL_TYPE_VAR_STRING);
        ENUM_TO_STRING(MYSQL_TYPE_STRING);
        ENUM_TO_STRING(MYSQL_TYPE_GEOMETRY);
    }
    return "UNKNOWN";
}

size_t get_storage_size(int type, size_t length)
{
    switch (type)
    {
        ENUM_2_STORAGE_STR(MYSQL_TYPE_DECIMAL, length);
        ENUM_2_STORAGE(MYSQL_TYPE_TINY, signed char);
        ENUM_2_STORAGE(MYSQL_TYPE_SHORT, short int);
        ENUM_2_STORAGE(MYSQL_TYPE_LONG, int);
        ENUM_2_STORAGE(MYSQL_TYPE_FLOAT, float);
        ENUM_2_STORAGE(MYSQL_TYPE_DOUBLE, double);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_NULL, length);
        ENUM_2_STORAGE(MYSQL_TYPE_TIMESTAMP, MYSQL_TIME);
        ENUM_2_STORAGE(MYSQL_TYPE_LONGLONG, long long int);
        ENUM_2_STORAGE(MYSQL_TYPE_INT24, int);
        ENUM_2_STORAGE(MYSQL_TYPE_DATE, MYSQL_TIME);
        ENUM_2_STORAGE(MYSQL_TYPE_TIME, MYSQL_TIME);
        ENUM_2_STORAGE(MYSQL_TYPE_DATETIME, MYSQL_TIME);
        ENUM_2_STORAGE(MYSQL_TYPE_YEAR, short int);
        ENUM_2_STORAGE(MYSQL_TYPE_NEWDATE, MYSQL_TIME);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_VARCHAR, length);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_BIT, length);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_NEWDECIMAL, length);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_ENUM, length);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_SET, length);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_TINY_BLOB, length);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_MEDIUM_BLOB, length);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_LONG_BLOB, length);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_BLOB, length);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_VAR_STRING, length);
        ENUM_2_STORAGE_STR(MYSQL_TYPE_STRING, length);
    }
    return 0;
}

int is_var_length_field(int type)
{
    switch(type)
    {
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_BIT:
        return 1;
    default:
        return 0;
    }
}

static int report_error(MYSQL *mysql)
{
    dprintf(STDERR_FILENO, "%s\n", mysql_error(mysql));
    mysql_close(mysql);
    return -1;
}

static int report_error_stmt(MYSQL *mysql, MYSQL_STMT *stmt)
{
    int ret = report_error(mysql);
    mysql_stmt_close(stmt);
    return ret;
}

/* static */
int mysql_dumper::dump(const char *dir,
                       const char *query,
                       size_t query_len,
                       struct db_login_info *login_info,
                       const char *db,
                       hashtable_vect<int> *types,
                       bool compress /* = true */)
{
    MYSQL mysql;
    MYSQL_STMT *stmt = NULL;
    int res = 0;
    mysql_init(&mysql);
    if (NULL == mysql_real_connect(&mysql, login_info->host, login_info->user, login_info->pass, db, login_info->port, NULL, compress ? CLIENT_COMPRESS : 0))
        return report_error(&mysql);

    my_bool b_flag = 0;
    if (0 != mysql_options(&mysql, MYSQL_REPORT_DATA_TRUNCATION, (const char *)&b_flag))
        return report_error(&mysql);
    
    stmt = mysql_stmt_init(&mysql);
    if (!stmt)
        return report_error(&mysql);

    if (0 != mysql_stmt_prepare(stmt, query, query_len))
        return report_error_stmt(&mysql, stmt);
    
    MYSQL_RES *rs = mysql_stmt_result_metadata(stmt);
    if (!rs)
        return report_error_stmt(&mysql, stmt);
    
    unsigned int n = mysql_num_fields(rs);
    MYSQL_FIELD *fields = mysql_fetch_fields(rs);
    int field_types[n];
    MYSQL_BIND bind[n];
    my_bool is_null[n];
    bool null_terminate_str[n];
    unsigned long length[n];

    mmap_file_write ffields[n];
    VarFieldWriter vfields[n];
    
    my_bool error[n];
    memset(bind, 0, sizeof(MYSQL_BIND) * n);
    memset(null_terminate_str, 0, sizeof(bool) * n);
    vmbuf buf;
    buf.init();
    buf.sprintf("%s/schema.txt", dir);
    int fdschema = creat(buf.data(), 0644);
    for (unsigned int i = 0; i < n; ++i)
    {
        buf.reset();
        buf.sprintf("%s/%s", dir, fields[i].name);

        field_types[i] = fields[i].type;
        bind[i].is_unsigned = IS_UNSIGNED(fields[i].flags);
        int64_t ds_type = -1;
        const char *ds_type_str = "VAR";
        if (NULL != types)
        {
            int *type = types->lookup(fields[i].name, strlen(fields[i].name));
            if (NULL != type)
            {
		null_terminate_str[i] = (MYSQLDUMPER_NULL_TERMINATED_STR == (*type & MYSQLDUMPER_NULL_TERMINATED_STR));
		int field_type = *type & ~(MYSQLDUMPER_NULL_TERMINATED_STR);
		bind[i].is_unsigned = (*type & MYSQLDUMPER_UNSIGNED) > 0 ? 1 : 0;
		field_type = field_type & ~(MYSQLDUMPER_UNSIGNED);
		if (0 != field_type)
		    field_types[i] = field_type;
            }
        }
        
        if (is_var_length_field(field_types[i]))
        {
            vfields[i].init(buf.data());
        } else
        {
            ds_type = get_ds_type(field_types[i], bind[i].is_unsigned);
            const char *s = ds_type_mapper::enum_to_str(ds_type);
            if (*s) ds_type_str = s;
            ffields[i].init(buf.data());
            ffields[i].write(&ds_type, sizeof(ds_type));
        }
        size_t len = get_storage_size(field_types[i], fields[i].length);
        dprintf(1, "%03d  name = %s, size=%zu, length=%lu, type=%s (%s), is_prikey=%d, ds_type=%s\n", i, fields[i].name, len, fields[i].length, get_type_name(field_types[i]), bind[i].is_unsigned ? "unsigned" : "signed", IS_PRI_KEY(fields[i].flags), ds_type_str);
        if (fdschema > 0)
            dprintf(fdschema, "%03d  name = %s, size=%zu, length=%lu, type=%s (%s), is_prikey=%d, ds_type=ds_field<%s> ds_%s\n", i, fields[i].name, len, fields[i].length, get_type_name(field_types[i]), bind[i].is_unsigned ? "unsigned" : "signed", IS_PRI_KEY(fields[i].flags), ds_type_str, fields[i].name);
        bind[i].buffer_type = (enum_field_types)field_types[i];
        bind[i].buffer_length = len;
        bind[i].buffer = malloc(len);
        bind[i].is_null = &is_null[i];
        bind[i].length = &length[i];
        bind[i].error = &error[i];
    }
    mysql_free_result(rs);
    ::close(fdschema);

    // execute
    if (0 != mysql_stmt_execute(stmt))
        return report_error_stmt(&mysql, stmt);

    // bind
    if (0 != mysql_stmt_bind_result(stmt, bind))
        return report_error_stmt(&mysql, stmt);

    size_t count = 0;
    // fetch
    char zeros[4096];
    memset(zeros, 0, sizeof(zeros));
    int err = 0;
    size_t num_rows_errors = 0;
    while (0 == (err = mysql_stmt_fetch(stmt)))
    {
        bool b = false;
        for (unsigned int i = 0; i < n && !b; ++i)
            b = b || error[i];
        if (b)
        {
            ++num_rows_errors;
            continue;
        }
                
        for (unsigned int i = 0; i < n; ++i)
        {
            if (is_var_length_field(field_types[i])) {
		unsigned long len = length[i];
		if (0 == is_null[i] && 0 < len && null_terminate_str[i]) {
		    if (bind[i].buffer_length <= len) { // should never happen. we allocate len+1 buf for var_len fields.
			dprintf(STDOUT_FILENO, "insufficent memory size to add null terminator to a string\n");
			return -1;
		    }
		    ((char *)bind[i].buffer)[len] = '\0';
		    len++;
		}
		vfields[i].write(bind[i].buffer, is_null[i] ? 0 : len);
	    } else
                ffields[i].write(is_null[i] ? zeros : bind[i].buffer, length[i]);
        }
        ++count;
    }
    if (err != MYSQL_NO_DATA)
    {
        dprintf(STDOUT_FILENO, "mysql_stmt_fetch returned an error (code=%d)\n", err);
        return err;
    }
    mysql_stmt_close(stmt);
    dprintf(STDOUT_FILENO, "%zu records, %zu errors (skipped)\n", count, num_rows_errors);

    for (unsigned int i = 0; i < n; ++i)
    {
        if (is_var_length_field(field_types[i]))
        {
            vfields[i].close();
        } else
        {
            ffields[i].close();
        }
    }    

    mysql_close(&mysql);
    return res;
}
