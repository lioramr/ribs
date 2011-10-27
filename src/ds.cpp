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
