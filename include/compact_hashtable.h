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
#ifndef _COMPACT_HASHTABLE__H_
#define _COMPACT_HASHTABLE__H_

#include "vmbuf.h"
#include <stdint.h>


template<typename K, typename V>
struct compact_hashtable_entry_t
{
    K k;
    V v;
    bool equals(const K &k1) const { return k == k1; }

    static uint32_t hash_code(const K &key)
    {
        register const unsigned char *p = (const unsigned char *)&key;
        register const unsigned char *end = p + sizeof(K);
        uint32_t h = 5381;
        for (; p != end; ++p)
            h = ((h << 5) + h) ^ *p;
        return h;
    }
};

// template specialization for strings
template<typename V>
struct compact_hashtable_entry_t<const char *, V>
{
    const char *k;
    V v;
    bool equals(const char *s) const { return 0 == strcmp(k, s); }
    static uint32_t hash_code(const char *key)
    {
        register const unsigned char *p = (const unsigned char *)key;
        uint32_t h = 5381;
        for (; *p; ++p)
            h = ((h << 5) + h) ^ *p;
        return h;
    }
};


template<typename K>
struct compact_hashtable_entry_no_val_t
{
    K k;
    bool equals(const K &k1) const { return k == k1; }
    static uint32_t hash_code(const K &key)
    {
        return compact_hashtable_entry_t<K, int>::hash_code(key);
    }
};

template<typename K=int, typename V=int, typename HTE=compact_hashtable_entry_t<K, V> >
struct compact_hashtable
{
    typedef uint32_t index_t;
    typedef HTE entry_t;
    struct internal_entry_t
    {
        index_t next;
        entry_t data;
    };
    
    void init(index_t num_buckets)
    {
        size = mask = 0;
        if (num_buckets < 8)
            num_buckets = 8;
        for (index_t m = (((index_t)-1) >> 1) + 1; m >= num_buckets; mask = m, m >>= 1);
        buckets.init();
        entries.init(mask * sizeof(internal_entry_t));
        buckets.alloczero(mask * sizeof(index_t));
        --mask;
    }

    index_t bucket(const K &k) const
    {
        return HTE::hash_code(k) & mask;
    }
        
    entry_t *insert(const K &k, const V &v)
    {
        index_t b = bucket(k);
        internal_entry_t *e = entries.alloc<internal_entry_t>();
        e->data.k = k;
        e->data.v = v;
        
        index_t *ofs_bucket_ptr = (index_t *)buckets.data() + b;
        e->next = *ofs_bucket_ptr;
        *ofs_bucket_ptr = ++size; // 1 based, zero is reserved
        return &e->data;
    }

    bool insert(const K &k)
    {
        index_t b = bucket(k);
        index_t *ofs_bucket_ptr = (index_t *)buckets.data() + b;
        register index_t index = *(ofs_bucket_ptr);
        while (index > 0)
        {
            internal_entry_t *e = (internal_entry_t *)entries.data() + index - 1; // index is 1 based, zero is reserved
            if (e->data.equals(k))
                return false;
            index = e->next;
        }
        
        internal_entry_t *e = entries.alloc<internal_entry_t>();
        e->data.k = k;
        
        e->next = *ofs_bucket_ptr;
        *ofs_bucket_ptr = ++size; // 1 based, zero is reserved
        return true;
    }

    entry_t *lookup(const K &k) const
    {
        index_t b = bucket(k);
        register index_t index = *(((index_t *)buckets.data()) + b);
        while (index > 0)
        {
            internal_entry_t *e = (internal_entry_t *)entries.data() + index - 1; // index is 1 based, zero is reserved
            if (e->data.equals(k))
                return &e->data;
            index = e->next;
        }
        return NULL;
    }

    entry_t *insert_unique(const K &k, const V &v)
    {
        entry_t *e = lookup(k);
        if (e)
            return e;
        return insert(k, v);
    }

    internal_entry_t *begin() { return (internal_entry_t *)entries.data(); }
    internal_entry_t *end() { return (internal_entry_t *)entries.wloc(); }

    void dump(const char *name, vmbuf *buf)
    {
        buf->sprintf("%s>\n", name);
        for (internal_entry_t *it = this->begin(), *itend = this->end(); it != itend; ++it)
        {
            buf->sprintf("   %u\n", it->data.k);
        }
    }

    index_t get_size() const { return this->size; }
    
    vmbuf buckets;
    vmbuf entries;
    index_t mask;
    index_t size;
};

template<typename K=int, typename HTE=compact_hashtable_entry_no_val_t<K> >
struct compact_hashset : compact_hashtable<K, int, HTE>
{
};

#endif // _COMPACT_HASHTABLE__H_
