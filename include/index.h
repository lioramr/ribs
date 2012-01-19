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
#ifndef _INDEX__H_
#define _INDEX__H_

#include "search.h"
#include "mmap_file.h"
#include "VarFieldReader.h"
#include "hashtable_file.h"
#include "vmbuf.h"
#include "logger.h"
#include "ds_field.h"
#include <stdlib.h>

/*
 * TODO:
 *    1) move vector size from lookup table to index
 *    2) write header
 */

template <typename T=uint32_t>
class index_generator
{
private:
    index_generator() {}
public:
    typedef struct
    {
        uint32_t id;
        T value;
    } fw;
    
    static int compar(const void *a, const void *b)
    {
        const fw *aa = (const fw *)a;
        const fw *bb = (const fw *)b;
        
        if (aa->value != bb->value)
            return aa->value < bb->value ? -1 : (aa->value > bb->value ? 1 : 0);
        
        return aa->id < bb->id ? -1 : (aa->id > bb->id ? 1 : 0);
    }

    static int generate(const char *filename)
    {
        ds_field<T> dsf;
        if (0 > dsf.init(filename))
            return -1;
        T *data = dsf.begin();
        T *data_end = dsf.end();
        uint32_t index = 0;
        vmbuf buf;
        buf.init();
        for (T *p = data; p != data_end; ++p, ++index)
        {
            fw f = { index, *p };
            buf.memcpy(&f, sizeof(f));
        }
        qsort(buf.data(), index, sizeof(fw), compar);
        fw *fs = (fw *)buf.data();
        fw *fe = fs + index;
        mmap_file_write mfw_lookup, mfw_index;
        size_t l = strlen(filename) + 20;
        char lookup_filename[l];
        char index_filename[l];
        sprintf(lookup_filename, "%s.lookup", filename);
        sprintf(index_filename, "%s.index", filename);
        if (0 > mfw_lookup.init(lookup_filename) ||
            0 > mfw_index.init(index_filename))
            return -1;
        for (fw *f = fs; f != fe; )
        {
            T v = f->value;
            uint32_t s = 0;
            size_t index_start, index_end;
            index_start = mfw_index.get_wloc() / sizeof(uint32_t);
            for (; f != fe && v == f->value; ++f, ++s)
            {
                mfw_index.write(&f->id, sizeof(uint32_t));
            }
            index_end = mfw_index.get_wloc() / sizeof(uint32_t);
            struct {
                T v;
                uint32_t ofs;
                uint32_t size;
            } lookup_entry = { v, index_start, index_end - index_start };
            mfw_lookup.write(&lookup_entry, sizeof(lookup_entry));
        }
        
        mfw_lookup.close();
        mfw_index.close();
        dsf.close();
        return 0;
    }
    
    static int generate(const char *dir, const char *name)
    {
        char buf[strlen(dir) + strlen(name) + 2];
        sprintf(buf, "%s/%s", dir, name);
        logger::log("indexing %s", buf);
        index_generator<T> index;
        int res = index.generate(buf);
        logger::log("indexing %s - DONE", buf);
        return res;
    }
};

struct var_index_generator
{
    typedef struct
    {
        uint32_t id;
        uint32_t value;
    } fw;

    static int compar(const void *a, const void *b)
    {
        const fw *aa = (const fw *)a;
        const fw *bb = (const fw *)b;
        
        if (aa->value != bb->value)
            return aa->value < bb->value ? -1 : (aa->value > bb->value ? 1 : 0);
        
        return aa->id < bb->id ? -1 : (aa->id > bb->id ? 1 : 0);
    }

    static int generate(const char *filename)
    {
        VarFieldReader vfr;
        if (0 > vfr.init(filename))
            return -1;

        size_t l = strlen(filename) + 20;

        hashtable_file ht_keys;
        char keys_filename[l];
        sprintf(keys_filename, "%s.keys", filename);
        if (0 > ht_keys.create(keys_filename))
            return -1;
        
        size_t *data = vfr.begin();
        size_t *data_end = vfr.end();
        
        uint32_t index = 0;
        vmbuf buf;
        buf.init();
        for (size_t *p = data; p != data_end; ++p, ++index)
        {
            char *d = vfr.get_data(p);
            size_t dsize = vfr.get_data_size(p);
            uint32_t ofs = ht_keys.lookup_create(d, dsize);
            uint64_t vect_ofs_size = 0;
            if (0 == ofs)
                ofs = ht_keys.insert(d, dsize, &vect_ofs_size, sizeof(vect_ofs_size));
            fw f = { index, ofs };
            buf.memcpy(&f, sizeof(f));
        }
        qsort(buf.data(), index, sizeof(fw), compar);
        fw *fs = (fw *)buf.data();
        fw *fe = fs + index;
        mmap_file_write mfw_lookup, mfw_index;
        
        char index_filename[l];
        sprintf(index_filename, "%s.index", filename);
        if (0 > mfw_index.init(index_filename))
            return -1;
        for (fw *f = fs; f != fe; )
        {
            uint32_t v = f->value;
            uint32_t s = 0;
            size_t index_start, index_end;
            index_start = mfw_index.get_wloc() / sizeof(uint32_t);
            for (; f != fe && v == f->value; ++f, ++s)
            {
                mfw_index.write(&f->id, sizeof(uint32_t));
            }
            index_end = mfw_index.get_wloc() / sizeof(uint32_t);
            uint64_t *val = (uint64_t *)ht_keys.get_val(v);
            *val = (index_end - index_start) + ((uint64_t)index_start << 32);
        }
        
        ht_keys.finalize();
        mfw_index.close();
        vfr.close();
        return 0;
    }
    
    static int generate(const char *dir, const char *name)
    {
        char buf[strlen(dir) + strlen(name) + 2];
        sprintf(buf, "%s/%s", dir, name);
        logger::log("indexing %s", buf);
        var_index_generator index;
        int res = index.generate(buf);
        logger::log("indexing %s - DONE", buf);
        return res;
    }
};

template<typename K=uint32_t>
class index_container
{
public:
    int init(const char *filename)
    {
        size_t l = strlen(filename) + 20;
        char lookup_filename[l];
        char index_filename[l];
        sprintf(lookup_filename, "%s.lookup", filename);
        sprintf(index_filename, "%s.index", filename);
        if (0 > mf_lookup.init(lookup_filename) || 0 > mf_index.init(index_filename))
            return -1;
        size = mf_lookup.size / sizeof(lookup_entry_t);
        return 0;
    }

    void close()
    {
        mf_lookup.close();
        mf_index.close();
    }

    typedef struct lookup_entry
    {
        K v;
        uint32_t ofs;
        uint32_t size;
        bool operator < (const struct lookup_entry &other) const
        {
            return v < other.v;
        }
    } lookup_entry_t;
    
    int lookup(K v, lookup_entry_t *result)
    {
        lookup_entry_t value = { v, 0, 0 };
        lookup_entry_t *data = (lookup_entry_t *)mf_lookup.mem_begin();
        const lookup_entry_t *res;
        if (search::binary(data, size, value, &res))
        {
            *result = *res;
            return 0;
        }
        return -1;
    }

    lookup_entry_t *get_lookup_table() { return (lookup_entry_t *)mf_lookup.mem_begin(); }
        

    uint32_t *get_index(lookup_entry_t *e)
    {
        return (uint32_t *)mf_index.mem_begin() + e->ofs;
    }

    uint32_t get_index_size(lookup_entry_t *e)
    {
        return e->size;
    }
    
    mmap_file mf_lookup;
    mmap_file mf_index;
    size_t size;
};

class var_index_container
{
public:
    int init(const char *filename)
    {
        size_t l = strlen(filename) + 20;
        char keys_filename[l];
        char index_filename[l];
        sprintf(keys_filename, "%s.keys", filename);
        sprintf(index_filename, "%s.index", filename);
        if (0 > ht_keys.load(keys_filename) || 0 > mf_index.init(index_filename))
            return -1;
        return 0;
    }

    size_t size() { return ht_keys.size; }

    int lookup(const void *key, size_t key_len, uint64_t *rec)
    {
        uint32_t ofs = ht_keys.lookup(key, key_len);
        if (0 == ofs)
            return -1;
        *rec = *(uint64_t *)ht_keys.get_val(ofs);
        return 0;
    }
    
    uint32_t *get_index(uint64_t rec)
    {
        return (uint32_t *)mf_index.mem_begin() + (rec >> 32);
    }

    uint32_t get_index_size(uint64_t rec)
    {
        return rec & 0xFFFFFFFF;
    }
    
    hashtable_file ht_keys;
    mmap_file mf_index;
};



#endif // _INDEX__H_
