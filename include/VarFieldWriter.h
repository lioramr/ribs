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
#ifndef _VAR_FIELD_WRITER__H_
#define _VAR_FIELD_WRITER__H_

#include "mmap_file.h"

class VarFieldWriter
{
public:
    inline int init(const char *base_filename);
    inline int store_ofs();
    inline int write(const void *buf, size_t count);
    inline int close();
    
    mmap_file_write mfw_ofs;
    mmap_file_write mfw_data;
};

inline int VarFieldWriter::init(const char *base_filename)
{
    size_t n = strlen(base_filename) + 1;
    char offs_fname[n + 5]; // .offs
    char data_fname[n + 5]; // .data
    sprintf(offs_fname, "%s.offs", base_filename);
    sprintf(data_fname, "%s.data", base_filename);

    if (0 > mfw_ofs.init(offs_fname))
        return -1;
        
    if (0 > mfw_data.init(data_fname))
        return -1;
        
    return 0;
}

inline int VarFieldWriter::store_ofs()
{
    size_t ofs = mfw_data.get_wloc();
    return mfw_ofs.write(&ofs, sizeof(ofs));
}

inline int VarFieldWriter::write(const void *buf, size_t count)
{
    if (0 > store_ofs())
        return -1;
    return mfw_data.write(buf, count);
}

inline int VarFieldWriter::close()
{
    int res = store_ofs(); // store the last offset so we can extract the length of the last record
    mfw_ofs.close();
    mfw_data.close();
    return res;
}




#endif // _VAR_FIELD_WRITER__H_
