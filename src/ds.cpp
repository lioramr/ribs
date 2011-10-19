#include "ds.h"

int load_ds(ds_to_load *ds, const char *path)
{
    vmbuf buf;
    logger::log("loading %s", path);
    for (; ds->name; ++ds)
    {
        buf.init();
        buf.sprintf("%s/%s", path, ds->name);
        if (ds->idx)
        {
            logger::log("index: loading %s", buf.data());
            if (0 > ds->idx->init(buf.data()))
                return -1;
        }
        if (ds->ds)
        {
            logger::log("ds: loading %s", buf.data());
            if (0 > ds->ds->init(buf.data()))
                return -1;
        }
        if (ds->link)
        {
            buf.strcpy(".link");
            logger::log("link: loading %s", buf.data());
            if (0 > ds->link->init(buf.data()))
                return -1;
        }
    }
    logger::log("done loading %s", path);
    logger::log("----------------------------------------");
    return 0;
}
