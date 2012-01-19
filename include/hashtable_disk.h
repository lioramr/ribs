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
#ifndef _HASHTABLE_DISK__H_
#define _HASHTABLE_DISK__H_

#include <stdint.h>
#include <stdlib.h>

#include "vmbuf.h"

struct hashtable_disk
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

    struct ht_header_t
    {
        uint32_t capacity;
        uint32_t mask;
        uint32_t size;
    };

    inline ht_header_t *header() const;
    inline void init_filenames(const char *basename);

    inline int init_create();
    inline int create(const char *basename);
    inline int create(int dat_fd, int bkt_fd);
    inline int create_mem();
    inline int load(const char *filename, int mmap_flags);

    inline int finalize();
    inline int close();

    inline uint32_t next_bucket(uint32_t bucket) const;

    static inline uint32_t hashcode(const void *key, size_t n);

    inline void resize_grow();
    inline void check_resize();

    inline uint32_t insert(const void *key, size_t key_len, const void *val, size_t val_len);
    inline uint32_t insert_unique(const void *key, size_t key_len, const void *val, size_t val_len);
    inline uint32_t insert_or_update(const void *key, size_t key_len, const void *val, size_t val_len);
    inline uint32_t insert(const char *key, const char *val);

    inline uint32_t lookup(const void *key, size_t key_len) const;
    inline const char *lookup(const char *key) const;
    inline uint32_t locate_new_slot(uint32_t bucket);
    inline void fix_chain_down(uint32_t bucket);
    inline int remove(const void *key, size_t key_len);

    inline void *get_key(uint32_t rec_ofs) const;
    inline uint32_t get_key_size(uint32_t rec_ofs) const;
    inline void *get_val(uint32_t rec_ofs) const;
    inline uint32_t get_val_size(uint32_t rec_ofs) const;

    uint32_t num_keys() const { return header()->size; }
    uint32_t mem_usage() { return buckets.wlocpos() + data.wlocpos(); }

    vmfile buckets;
    vmfile data;

    // |dat loc|bkt loc|tmp loc|data_filename.dat\0|bucket_filename.bkt\0|tmp_filename.tmp\0
    enum
    {
        FN_DAT,
        FN_BKT,
        FN_TMP
    };
    const char *get_filename(int i) { return filename.data(*(((uint16_t *)filename.data()) + i)); }
    vmbuf filename;
};

inline hashtable_disk::ht_header_t *hashtable_disk::header() const
{
    return (ht_header_t *)data.data();
}

inline void hashtable_disk::init_filenames(const char *basename)
{
    filename.init();
    filename.wseek(sizeof(uint16_t) * 3);

    uint16_t *filename_ofs = (uint16_t *)filename.data();

    *filename_ofs++ = filename.wlocpos(); // dat location
    filename.strcpy(basename);
    filename.strcpy(".dat");
    filename.copy<char>('\0');

    *filename_ofs++ = filename.wlocpos(); // bkt location
    filename.strcpy(basename);
    filename.strcpy(".bkt");
    filename.copy<char>('\0');

    *filename_ofs++ = filename.wlocpos(); // tmp location
    filename.strcpy(basename);
    filename.strcpy(".tmp");
    filename.copy<char>('\0');
}

inline int hashtable_disk::init_create()
{
    struct ht_header_t header;
    header.capacity = INITIAL_CAPACITY;
    header.mask = header.capacity - 1;
    header.size = 0;

    data.copy<ht_header_t>(header);
    size_t n = header.capacity * sizeof(struct entry_t);
    buckets.resize_if_less(n);
    buckets.wseek(n);
    return 0;
}

inline int hashtable_disk::create(const char *basename)
{
    init_filenames(basename);
    if (0 > data.create(get_filename(FN_DAT)) || 0 >  buckets.create(get_filename(FN_BKT)))
        return -1;

    return init_create();
}

inline int hashtable_disk::create_mem()
{
    if (0 > data.create_tmp() || 0 > buckets.create_tmp())
        return -1;

    return init_create();
}

inline int hashtable_disk::load(const char *basename, int mmap_flags)
{
    init_filenames(basename);
    if (0 > data.load(get_filename(FN_DAT), mmap_flags, VMSTORAGE_RW) ||
        0 > buckets.load(get_filename(FN_BKT), mmap_flags, VMSTORAGE_RW))
        return -1;

    return 0;
}

inline int hashtable_disk::finalize()
{
    return ((buckets.finalize() + data.finalize()) == 0 ? 0 : -1);
}

inline int hashtable_disk::close()
{
    return ((buckets.free() + data.free()) == 0 ? 0 : -1);
}

inline uint32_t hashtable_disk::next_bucket(uint32_t bucket) const
{
    ++bucket;
    if (bucket == header()->capacity)
        bucket = 0;
    return bucket;
}

/* static */
inline uint32_t hashtable_disk::hashcode(const void *key, size_t n)
{
    register const unsigned char *p = (const unsigned char *)key;
    register const unsigned char *end = p + n;
    uint32_t h = 5381;
    for (; p != end; ++p)
        h = ((h << 5) + h) ^ *p;
    return h;
}

/* static */
/*
inline uint32_t hashtable_disk::hashcode(const void *key, size_t n)
{
    register const unsigned char *p = (const unsigned char *)key;
    register const unsigned char *end = p + n;
    register uint32_t h = 0;
    const uint32_t prime = 0x811C9DC5;
    for (; p != end; ++p)
        h = (h ^ *p) * prime;
    return h;
}
*/

inline void hashtable_disk::resize_grow()
{
    uint32_t new_capacity = header()->capacity << 1;
    uint32_t new_mask = new_capacity - 1;

    vmfile new_buckets;
    new_buckets.create(get_filename(FN_TMP), sizeof(struct entry_t) * new_capacity);
    new_buckets.wseek(sizeof(struct entry_t) * new_capacity);

    struct entry_t *entries = (struct entry_t *)new_buckets.data();
    for (entry_t *e = (struct entry_t *)buckets.data(), *end = e + header()->capacity; e != end; ++e) {
        uint32_t new_bucket = e->hashcode & new_mask;
        for (;;) {
            struct entry_t *ne = entries + new_bucket;
            if (ne->rec_ofs == 0) {
                *ne = *e;
                break;
            }
            ++new_bucket;
            if (new_bucket == new_capacity)
                new_bucket = 0;
        }
    }
    buckets.free();
    buckets = new_buckets;
    new_buckets.storage.detach();
    if (0 > rename(get_filename(FN_TMP), get_filename(FN_BKT))) {
        perror("hashtable_disk::resize_grow() failed to rename tmp -> bkt");
        abort();
    }

    header()->capacity = new_capacity;
    header()->mask = new_mask;
}

inline void hashtable_disk::check_resize()
{
    uint32_t c = header()->capacity >> 1;
    if (header()->size > c)
        resize_grow();
}

inline uint32_t hashtable_disk::insert(const void *key, size_t key_len, const void *val, size_t val_len)
{
    check_resize();
    uint32_t hc = hashcode(key, key_len);
    uint32_t bucket = hc & header()->mask;
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
            ++header()->size;
            return ofs;
        } else {
            bucket = next_bucket(bucket);
        }
    }
    return 0;
}

inline uint32_t hashtable_disk::insert_unique(const void *key, size_t key_len, const void *val, size_t val_len)
{
    check_resize();
    uint32_t hc = hashcode(key, key_len);
    uint32_t bucket = hc & header()->mask;
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
            ++header()->size;
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
        bucket = next_bucket(bucket);
    } 
    return 0;
}

inline uint32_t hashtable_disk::insert_or_update(const void *key, size_t key_len, const void *val, size_t val_len)
{
    check_resize();
    uint32_t hc = hashcode(key, key_len);
    uint32_t bucket = hc & header()->mask;
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
            ++header()->size;
            return ofs;
        }
        if (hc == e->hashcode)
        {
            uint32_t ofs = e->rec_ofs;
            char *rec = data.data(ofs);
            char *k = rec + (sizeof(uint32_t) * 2);
            if (*(uint32_t *)rec == key_len && 0 == memcmp(key, k, key_len))
            {
                uint32_t *vl = (uint32_t *)rec + 1;
                if (val_len <= *vl)
                {
                    memcpy(k + key_len, val, val_len);
                    *vl = val_len;
                } else
                {
                    ofs = data.wlocpos();
                    e->rec_ofs = ofs;
                    *data.alloc<uint32_t>() = key_len;
                    *data.alloc<uint32_t>() = val_len;
                    data.memcpy(key, key_len);
                    data.memcpy(val, val_len);
                }
                return ofs;
            }
        }
        bucket = next_bucket(bucket);
    } 
    return 0; // should never get here
}


inline uint32_t hashtable_disk::insert(const char *key, const char *val)
{
    return insert(key, strlen(key), val, strlen(val)+1);
}

inline uint32_t hashtable_disk::lookup(const void *key, size_t key_len) const
{
    uint32_t hc = hashcode(key, key_len);
    uint32_t bucket = hc & header()->mask;
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
        bucket = next_bucket(bucket);
    } 
    return 0;
}

inline const char *hashtable_disk::lookup(const char *key) const
{
    uint32_t res = lookup(key, strlen(key));
    if (0 == res)
        return "";
    else
        return (const char *)get_val(res);
}

inline uint32_t hashtable_disk::locate_new_slot(uint32_t current_bucket)
{
    struct entry_t *entries = (struct entry_t *)buckets.data();
    uint32_t bucket = (entries + current_bucket)->hashcode & header()->mask; // ideal slot
    for (;;)
    {
        if (bucket == current_bucket) // same slot
            break;
        struct entry_t *e = entries + bucket;
        if (e->rec_ofs == 0) // new slot
            break;
        bucket = next_bucket(bucket);
    }
    return bucket;
}

inline void hashtable_disk::fix_chain_down(uint32_t bucket)
{
    struct entry_t *entries = (struct entry_t *)buckets.data();
    for (;;)
    {
        struct entry_t *e = entries + bucket;
        if (e->rec_ofs == 0)
            break;
        
        uint32_t new_bucket = locate_new_slot(bucket);
        if (new_bucket != bucket)
        {
            // move bucket to new place
            *(entries + new_bucket) = *e;
            e->rec_ofs = 0;
        }
        bucket = next_bucket(bucket);
    }
}

inline int hashtable_disk::remove(const void *key, size_t key_len)
{
    uint32_t hc = hashcode(key, key_len);
    uint32_t bucket = hc & header()->mask;
    struct entry_t *entries = (struct entry_t *)buckets.data();
    for (;;)
    {
        struct entry_t *e = entries + bucket;
        if (e->rec_ofs == 0)
            return -1;
        if (hc == e->hashcode)
        {
            uint32_t ofs = e->rec_ofs;
            char *rec = data.data(ofs);
            char *k = rec + (sizeof(uint32_t) * 2);
            if (*(uint32_t *)rec == key_len && 0 == memcmp(key, k, key_len))
            {
                e->rec_ofs = 0;
                bucket = next_bucket(bucket);
                fix_chain_down(bucket);
                --header()->size;
                return 0;
            }
        }
        bucket = next_bucket(bucket);
    } 
    return -1; // not found
}

inline void *hashtable_disk::get_key(uint32_t rec_ofs) const
{
    char *rec = data.data(rec_ofs);
    return rec + (sizeof(uint32_t) * 2);
}

inline uint32_t hashtable_disk::get_key_size(uint32_t rec_ofs) const
{
    char *rec = data.data(rec_ofs);
    return *((uint32_t *)rec);
}

inline void *hashtable_disk::get_val(uint32_t rec_ofs) const
{
    char *rec = data.data(rec_ofs);
    return rec + (sizeof(uint32_t) * 2) + *(uint32_t *)rec;
}

inline uint32_t hashtable_disk::get_val_size(uint32_t rec_ofs) const
{
    char *rec = data.data(rec_ofs);
    return *((uint32_t *)rec + 1);
}

#endif // _HASHTABLE_DISK__H_
