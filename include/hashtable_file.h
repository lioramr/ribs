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
#ifndef _HASHTABLE_FILE__H_
#define _HASHTABLE_FILE__H_

#include <stdint.h>
#include "vmbuf.h"
#include "tempfd.h"

struct hashtable_file
{
    enum
    {
        INITIAL_CAPACITY = 256
    };

    struct entry_t
    {
        uint32_t hashcode;
        uint32_t rec_ofs;
    };

    inline int init_create();
    inline int create(const char *filename);
    inline int create(int fd);
    inline int create_mem();
    inline int load(const char *filename, int mmap_flags);
    inline int load_readonly(const char *filename, int mmap_flags);
    inline int load_readwrite(const char *filename, int mmap_flags);
    
    inline int finalize();
    inline int close();
    
    static inline uint32_t hashcode(const void *key, size_t n);
    
    inline void resize_grow();
    inline void check_resize();

    inline uint32_t insert(const void *key, size_t key_len, const void *val, size_t val_len);
    inline uint32_t insert_unique(const void *key, size_t key_len, const void *val, size_t val_len);
    inline uint32_t insert(const char *key, const char *val);
    
    inline uint32_t lookup_create(const void *key, size_t key_len) const;
    inline const char *lookup_create(const char *key) const;
    inline uint32_t lookup(const void *key, size_t key_len) const;
    inline const char *lookup(const char *key) const;
    
    inline void *get_key(uint32_t rec_ofs) const;
    inline uint32_t get_key_size(uint32_t rec_ofs) const;
    inline void *get_val(uint32_t rec_ofs) const;
    inline uint32_t get_val_size(uint32_t rec_ofs) const;
    
    uint32_t ofs_buckets;
    uint32_t capacity;
    uint32_t mask;
    uint32_t size;

    vmbuf buckets;
    vmfile data;
};

/*
 * inline
 */
inline int hashtable_file::init_create()
{
    capacity = INITIAL_CAPACITY;
    mask = capacity - 1;
    size = 0;
    
    data.wseek(sizeof(uint32_t) * 3); // offset,num_elements,size of buckets table
    size_t n = capacity * sizeof(struct entry_t);
    if (0 > buckets.init(n))
        return -1;
    buckets.wseek(n);
    return 0;
}

inline int hashtable_file::create(const char *filename)
{
    if (0 > data.create(filename))
        return -1;
        
    return init_create();
}

inline int hashtable_file::create(int fd)
{
    if (0 > data.create(fd))
        return -1;
        
    return init_create();
}

inline int hashtable_file::create_mem()
{
    if (0 > data.create_tmp())
        return -1;
    
    return init_create();
}

inline int hashtable_file::load(const char *filename, int mmap_flags)
{
	return load_readonly(filename, mmap_flags);
}

inline int hashtable_file::load_readonly(const char *filename, int mmap_flags)
{
    if (0 > data.load(filename, mmap_flags, VMSTORAGE_RO))
        return -1;
    uint32_t *header = (uint32_t *)data.data();
    ofs_buckets = *header++;
    size = *header++;
    capacity = *header;
    mask = capacity - 1;
    return 0;
}

/**
 * In addition to loading the data and setting size, capacity, mask, etc.
 * this method copies the  buckets into memory and prepares the hashtable
 * for new inserts.
 */
inline int hashtable_file::load_readwrite(const char *filename, int mmap_flags)
{
    if (0 > data.load(filename, mmap_flags, VMSTORAGE_RW))
        return -1;
    uint32_t *header = (uint32_t *)data.data();
    ofs_buckets = *header++;
    size = *header++;
    capacity = *header;
    mask = capacity - 1;

    size_t n = capacity * sizeof(struct entry_t);
    if (0 > buckets.init(n))
    	return -1;

    buckets.memcpy(data.data(ofs_buckets), n);
    data.reset();
    data.wseek(ofs_buckets);
    return 0;
}

inline int hashtable_file::finalize()
{
    uint32_t ofs = data.wlocpos();
    uint32_t *header = (uint32_t *)data.data();
    *header++ = ofs;
    *header++ = size;
    *header = capacity;
    int res = data.memcpy(buckets.data(), buckets.capacity());
    buckets.free();
    if (0 == res)
        res = data.finalize(); // truncate to final size
    return res;
}

inline int hashtable_file::close()
{
    return ((buckets.free() + data.free()) == 0 ? 0 : -1);
}

/* static */
inline uint32_t hashtable_file::hashcode(const void *key, size_t n)
{
    register const unsigned char *p = (const unsigned char *)key;
    register const unsigned char *end = p + n;
    uint32_t h = 5381;
    for (; p != end; ++p)
        h = ((h << 5) + h) ^ *p;
    return h;
}

inline void hashtable_file::resize_grow()
{
    vmbuf buf;
    uint32_t new_capacity = capacity << 1;
    uint32_t new_mask = new_capacity - 1;
    buf.init(new_capacity * sizeof(struct entry_t));
    struct entry_t *entries = (struct entry_t *)buf.data();
        
    for (entry_t *e = (struct entry_t *)buckets.data(), *end = e + capacity; e != end; ++e)
    {
        uint32_t new_bucket = e->hashcode & new_mask;
        for (;;)
        {
            struct entry_t *ne = entries + new_bucket;
            if (ne->rec_ofs == 0)
            {
                *ne = *e;
                break;
            }
            ++new_bucket;
            if (new_bucket == new_capacity)
                new_bucket = 0;
        }
    }
    buckets.free();
    buckets = buf;
    buf.detach();
    capacity = new_capacity;
    mask = new_mask;
}
    
inline void hashtable_file::check_resize()
{
    uint32_t c = capacity >> 1;
    if (size > c)
        resize_grow();
}
       

inline uint32_t hashtable_file::insert(const void *key, size_t key_len, const void *val, size_t val_len)
{
    check_resize();
    uint32_t hc = hashcode(key, key_len);
    uint32_t bucket = hc & mask;
    struct entry_t *entries = (struct entry_t *)buckets.data();
    for (;;)
    {
        struct entry_t *e = entries + bucket;
        if (0 == e->rec_ofs)
        {
            e->hashcode = hc;
            uint32_t ofs = data.wlocpos();
            e->rec_ofs = ofs;
            *data.alloc<uint32_t>() = key_len;
            *data.alloc<uint32_t>() = val_len;
            data.memcpy(key, key_len);
            data.memcpy(val, val_len);
            ++size;
            return ofs;
        } else
        {
            ++bucket;
            if (bucket == capacity)
                bucket = 0;
        }
    }
    return 0;
}

inline uint32_t hashtable_file::insert_unique(const void *key, size_t key_len, const void *val, size_t val_len)
{
	check_resize();
    uint32_t hc = hashcode(key, key_len);
    uint32_t bucket = hc & mask;
    struct entry_t *entries = (struct entry_t *)buckets.data();
    for (;;)
    {
        struct entry_t *e = entries + bucket;
        if (e->rec_ofs == 0)
        {
            e->hashcode = hc;
            uint32_t ofs = data.wlocpos();
            e->rec_ofs = ofs;
            *data.alloc<uint32_t>() = key_len;
            *data.alloc<uint32_t>() = val_len;
            data.memcpy(key, key_len);
            data.memcpy(val, val_len);
            ++size;
            return ofs;
        }
        if (hc == e->hashcode)
        {
            uint32_t ofs = e->rec_ofs;
            char *rec = data.data(ofs);
            char *k = rec + (sizeof(uint32_t) * 2);
            if (*(uint32_t *)rec == key_len && 0 == memcmp(key, k, key_len))
                return ofs;
        }
        ++bucket;
        if (bucket == capacity)
            bucket = 0;
    } 
    return 0;
}

inline uint32_t hashtable_file::insert(const char *key, const char *val)
{
    return insert(key, strlen(key), val, strlen(val)+1);
}

inline uint32_t hashtable_file::lookup_create(const void *key, size_t key_len) const
{
    uint32_t hc = hashcode(key, key_len);
    uint32_t bucket = hc & mask;
    struct entry_t *entries = (struct entry_t *)buckets.data();
    for (;;)
    {
        struct entry_t *e = entries + bucket;
        if (e->rec_ofs == 0)
            return 0;
        if (hc == e->hashcode)
        {
            uint32_t ofs = e->rec_ofs;
            char *rec = data.data(ofs);
            char *k = rec + (sizeof(uint32_t) * 2);
            if (*(uint32_t *)rec == key_len && 0 == memcmp(key, k, key_len))
                return ofs;
        }
        ++bucket;
        if (bucket == capacity)
            bucket = 0;
    } 
    return 0;
}

inline const char *hashtable_file::lookup_create(const char *key) const
{
    uint32_t res = lookup_create(key, strlen(key));
    if (0 == res)
        return "";
    else
        return (const char *)get_val(res);
}

inline uint32_t hashtable_file::lookup(const void *key, size_t key_len) const
{
    uint32_t hc = hashcode(key, key_len);
    uint32_t bucket = hc & mask;
    struct entry_t *entries = (struct entry_t *)data.data(ofs_buckets);
    for (;;)
    {
        struct entry_t *e = entries + bucket;
        if (e->rec_ofs == 0)
            return 0;
        if (hc == e->hashcode)
        {
            uint32_t ofs = e->rec_ofs;
            char *rec = data.data(ofs);
            char *k = rec + (sizeof(uint32_t) * 2);
            if (*(uint32_t *)rec == key_len && 0 == memcmp(key, k, key_len))
                return ofs;
        }
        ++bucket;
        if (bucket == capacity)
            bucket = 0;
    } 
    return 0;
}

inline const char *hashtable_file::lookup(const char *key) const
{
    uint32_t res = lookup(key, strlen(key));
    if (0 == res)
        return "";
    else
        return (const char *)get_val(res);
}

inline void *hashtable_file::get_key(uint32_t rec_ofs) const
{
    char *rec = data.data(rec_ofs);
    return rec + (sizeof(uint32_t) * 2);
}

inline uint32_t hashtable_file::get_key_size(uint32_t rec_ofs) const
{
    char *rec = data.data(rec_ofs);
    return *((uint32_t *)rec);
}

inline void *hashtable_file::get_val(uint32_t rec_ofs) const
{
    char *rec = data.data(rec_ofs);
    return rec + (sizeof(uint32_t) * 2) + *(uint32_t *)rec;
}
    
inline uint32_t hashtable_file::get_val_size(uint32_t rec_ofs) const
{
    char *rec = data.data(rec_ofs);
    return *((uint32_t *)rec + 1);
}

#endif // _HASHTABLE_FILE__H_
