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
#ifndef _VAR_FIELD_READER__H_
#define _VAR_FIELD_READER__H_

#include "mmap_file.h"
#include <string.h>

class VarFieldReader
{
public:
    typedef struct record
    {
        char *data;
        size_t size;
    } record_t;
    
    inline int init(const char *base_filename);
    inline record_t get(size_t index);
    inline void close();
    size_t num_records()
    {
	size_t n = mf_ofs.mem_size() / sizeof(size_t);
	return (n > 0)?n-1:0; // the -1 because of the dummy entry at the end of the mf_ofs
    }

    size_t *begin() { return (size_t *)mf_ofs.mem_begin(); }
    size_t *end() { return (size_t *)mf_ofs.mem_end() - 1; } // the last ofs is there only for the size

    char *get_data(size_t *ofs) { return mf_data.mem_begin() + *ofs; }
    size_t get_data_size(size_t *ofs) { return *(ofs + 1) - *ofs; }
        
    
    mmap_file mf_ofs;
    mmap_file mf_data;
};


inline int VarFieldReader::init(const char *base_filename)
{
    size_t n = strlen(base_filename) + 1;
    char offs_fname[n + 5]; // .offs
    char data_fname[n + 5]; // .data
    sprintf(offs_fname, "%s.offs", base_filename);
    sprintf(data_fname, "%s.data", base_filename);
        
    if (0 > mf_ofs.init(offs_fname) || 0 > mf_data.init(data_fname))
        return -1;
    return 0;
}

inline VarFieldReader::record_t VarFieldReader::get(size_t index)
{
    size_t *ofs = (size_t *)mf_ofs.mem_begin() + index;
    record_t rec = { mf_data.mem_begin() + *ofs, *(ofs + 1) - *ofs };
    return rec;
}

inline void VarFieldReader::close()
{
    mf_ofs.close();
    mf_data.close();
}



#endif // _VAR_FIELD_READER__H_
