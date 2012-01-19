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
#ifndef _DS_FIELD__H_
#define _DS_FIELD__H_

#include "mmap_file.h"
#include "ds_type_mapper.h"

struct ds_field_base
{
    virtual ~ds_field_base() {}
    virtual int init(const char *filename) = 0;
};

struct ds_field_base_write
{
    virtual ~ds_field_base_write() {}
    virtual int init(const char *filename) = 0;
};

template<typename T>
struct ds_field : ds_field_base
{
    mmap_file mmf;
    
    int init(const char *filename)
    {
        if (0 > mmf.init(filename))
            return -1;
        int64_t t = *(int64_t *)mmf.mem_begin();
        if (!ds_type_mapper::is_same_type((T *)NULL, t))
        {
            LOGGER_ERROR("%s : not the same type ([code] %s != [file] %s)", filename, ds_type_mapper::type_to_str((T *)NULL), ds_type_mapper::enum_to_str(t));
            return -1;
        }
        return 0;
    }
    int close() { return mmf.close(); }

    size_t num_records() { return end() - begin(); }

    int get_val_safe(size_t index, T &out_val)
    {
        if (index >= num_records())
            return -1;
        out_val = *(begin() + index);
        return 0;
    }

    T get_val(size_t index)
    {
        return *(begin() + index);
    }

    template<typename E>
    int get_enum_safe(size_t index, E &out_val)
    {
        return get_val_convert(index, out_val);
    }

    template<typename E>
    int get_val_convert(size_t index, E &out_val)
    {
        T v = 0;
        int res = get_val_safe(index, v);
        out_val = (E)v;
        return res;
    }
        
    T *begin() { return (T *)(mmf.mem_begin() + sizeof(int64_t)); }
    T *end() { return (T *)mmf.mem_end(); }

};

template<typename T>
struct ds_field_write : ds_field_base_write
{
    mmap_file_write mmfw;
    int init(const char *filename)
    {
        if (0 > mmfw.init(filename))
            return -1;
        
        int64_t t = ds_type_mapper::type_to_enum((T *)NULL);
        return mmfw.write(&t, sizeof(t));
    }
    
    ssize_t write(const T &v)
    {
        return mmfw.write(&v, sizeof(T));
    }

    int close() { return mmfw.close(); }
};

#endif // _DS_FIELD__H_
