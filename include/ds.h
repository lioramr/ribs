#ifndef _DS__H_
#define _DS__H_

#include "ds_field.h"
#include "index.h"

struct ds_to_load
{
    index_container<> *idx;
    ds_field_base *ds;
    ds_field_base *link;
    const char *name;
};

int load_ds(ds_to_load *ds, const char *path);

#endif // _DS__H_
