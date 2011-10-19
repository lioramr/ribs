#ifndef _HASHTABLE__H_
#define _HASHTABLE__H_

#include "vmbuf.h"
#include <stdint.h>

struct hashtable
{
    struct entry_t
    {
        uint32_t next; // must be first for the following tricks to work
        uint32_t key;
        uint32_t key_len;
        uint32_t val;
        uint32_t val_len;
    };

    enum
    {
        DEFAULT_NUM_BUCKETS = 64
    };

    void init(uint32_t n = DEFAULT_NUM_BUCKETS);
    
    uint32_t bucket(const void *key, uint32_t n);
    
    uint32_t insert(const void *key, uint32_t key_len, const void *val, uint32_t val_len);
    uint32_t insert(uint32_t bucket, const void *key, uint32_t key_len, const void *val, uint32_t val_len);
    void insert(uint32_t key_ofs, const void *val, uint32_t val_len);
    uint32_t insert(const char *key, const char *val);

    uint32_t lookup(const void *key, uint32_t key_len);
    uint32_t lookup(const char *key);
    bool lookup_insert(const void *key, uint32_t key_len, void *val, uint32_t val_len);

    void insert32(const void *key, uint32_t key_len, uint32_t val);
    uint32_t *lookup32(const void *key, uint32_t key_len);
    
    void remove(char *key, uint32_t key_len);
    
    bool is_found(uint32_t ofs_entry);
    char *get_key(uint32_t ofs_entry);
    uint32_t get_key_len(uint32_t ofs_entry);
    char *get_val(uint32_t ofs_entry);
    uint32_t get_val_len(uint32_t ofs_entry);
    
    vmbuf buf;
    uint32_t mask;
    uint32_t size;
};


/*
 * inline functions
 */

inline void hashtable::init(uint32_t n /* = DEFAULT_NUM_BUCKETS */)
{
    for (uint32_t m = (((uint32_t)-1) >> 1) + 1; m >= n; mask = m, m >>= 1);
    buf.init();
    buf.alloc(mask * sizeof(uint32_t)); // will always return 0, no need to store the ofs
    memset(buf.data(), 0, mask * sizeof(uint32_t));
    --mask;
    size = 0;
}
        
inline uint32_t hashtable::bucket(const void *key, uint32_t n)
{
    register const unsigned char *p = (const unsigned char *)key;
    register const unsigned char *end = p + n;
    uint32_t h = 5381;
    for (; p != end; ++p)
        h = ((h << 5) + h) ^ *p;
    return h & mask;
}

inline uint32_t hashtable::insert(const void *key, uint32_t key_len, const void *val, uint32_t val_len)
{
    uint32_t b = bucket(key, key_len);
    return insert(b, key, key_len, val, val_len);
}

inline uint32_t hashtable::insert(uint32_t bucket, const void *key, uint32_t key_len, const void *val, uint32_t val_len)
{
    uint32_t ofs_key = buf.alloc(key_len);
    uint32_t ofs_val = buf.alloc(val_len);
    uint32_t ofs_entry = buf.alloc(sizeof(struct entry_t));

    memcpy(buf.data() + ofs_key, key, key_len);
    memcpy(buf.data() + ofs_val, val, val_len);
        
    struct entry_t *e = (struct entry_t *)(buf.data() + ofs_entry);
    e->key = ofs_key;
    e->key_len = key_len;
    e->val = ofs_val;
    e->val_len = val_len;

    uint32_t *ofs_bucket_ptr = (((uint32_t *)buf.data()) + bucket);
    e->next = *ofs_bucket_ptr;
    *ofs_bucket_ptr = ofs_entry;
    ++size;
    return ofs_entry;
}

inline void hashtable::insert(uint32_t key_ofs, const void *val, uint32_t val_len)
{
    uint32_t key_len = buf.wlocpos() - key_ofs;
    void *key = buf.data(key_ofs);
    uint32_t b = bucket(key, key_len);
    uint32_t ofs_val = buf.alloc(val_len);
    uint32_t ofs_entry = buf.alloc(sizeof(struct entry_t));

    memcpy(buf.data() + ofs_val, val, val_len);
        
    struct entry_t *e = (struct entry_t *)(buf.data() + ofs_entry);
    e->key = key_ofs;
    e->key_len = key_len;
    e->val = ofs_val;
    e->val_len = val_len;

    uint32_t *ofs_bucket_ptr = (((uint32_t *)buf.data()) + b);
    e->next = *ofs_bucket_ptr;
    *ofs_bucket_ptr = ofs_entry;
    ++size;
}

inline uint32_t hashtable::insert(const char *key, const char *val)
{
    return insert(key, strlen(key), val, strlen(val)+1);
}

inline uint32_t hashtable::lookup(const void *key, uint32_t key_len)
{
    uint32_t b = bucket(key, key_len);
    uint32_t ofs_entry = *(((uint32_t *)buf.data()) + b);
    while (ofs_entry > 0)
    {
        struct entry_t *e = (struct entry_t *)(buf.data() + ofs_entry);
        if (e->key_len == key_len && 0 == memcmp(buf.data() + e->key, key, key_len))
            return ofs_entry;
        ofs_entry = e->next;
    }
    return 0;
}

inline bool hashtable::lookup_insert(const void *key, uint32_t key_len, void *val, uint32_t val_len)
{
    uint32_t b = bucket(key, key_len);
    uint32_t ofs_entry = *(((uint32_t *)buf.data()) + b);
    while (ofs_entry > 0)
    {
        struct entry_t *e = (struct entry_t *)(buf.data() + ofs_entry);
        if (e->key_len == key_len && 0 == memcmp(buf.data() + e->key, key, key_len))
        {
            memcpy(val, buf.data() + e->val, e->val_len);
            return false;
        }
        ofs_entry = e->next;
    }
    insert(b, key, key_len, val, val_len);
    return true;
}

inline uint32_t hashtable::lookup(const char *key)
{
    return lookup(key, strlen(key));
}


inline void hashtable::insert32(const void *key, uint32_t key_len, uint32_t val)
{
    uint32_t b = bucket(key, key_len);
    uint32_t ofs_key = buf.alloc(key_len);
    uint32_t ofs_entry = buf.alloc(sizeof(struct entry_t));

    memcpy(buf.data() + ofs_key, key, key_len);
        
    struct entry_t *e = (struct entry_t *)(buf.data() + ofs_entry);
    e->key = ofs_key;
    e->key_len = key_len;
    e->val = val;

    uint32_t *ofs_bucket_ptr = (((uint32_t *)buf.data()) + b);
    e->next = *ofs_bucket_ptr;
    *ofs_bucket_ptr = ofs_entry;
    ++size;
}

inline uint32_t *hashtable::lookup32(const void *key, uint32_t key_len)
{
    uint32_t b = bucket(key, key_len);
    uint32_t ofs_entry = *(((uint32_t *)buf.data()) + b);
    while (ofs_entry > 0)
    {
        struct entry_t *e = (struct entry_t *)(buf.data() + ofs_entry);
        if (e->key_len == key_len && 0 == memcmp(buf.data() + e->key, key, key_len))
            return &e->val;
        ofs_entry = e->next;
    }
    return NULL;
}

inline void hashtable::remove(char *key, uint32_t key_len)
{
    uint32_t b = bucket(key, key_len);
    uint32_t ofs_entry_prev = (sizeof(uint32_t) * b);
    uint32_t ofs_entry = *((uint32_t *)(buf.data() + ofs_entry_prev));
        
    while (ofs_entry > 0)
    {
        struct entry_t *e = (struct entry_t *)(buf.data() + ofs_entry);
        if (e->key_len == key_len && 0 == memcmp(buf.data() + e->key, key, key_len))
        {
            ((struct entry_t *)(buf.data() + ofs_entry_prev))->next = e->next; // trick, next must be first
            return;
        }
        ofs_entry_prev = ofs_entry;
        ofs_entry = e->next;
    }
}

inline bool hashtable::is_found(uint32_t ofs_entry)
{
    return ofs_entry > 0;
}

inline char *hashtable::get_key(uint32_t ofs_entry)
{
    return buf.data() + ((struct entry_t *)(buf.data() + ofs_entry))->key;
}

inline uint32_t hashtable::get_key_len(uint32_t ofs_entry)
{
    return ((struct entry_t *)(buf.data() + ofs_entry))->key_len;
}

inline char *hashtable::get_val(uint32_t ofs_entry)
{
    return buf.data() + ((struct entry_t *)(buf.data() + ofs_entry))->val;
}

inline uint32_t hashtable::get_val_len(uint32_t ofs_entry)
{
    return ((struct entry_t *)(buf.data() + ofs_entry))->val_len;
}

#endif // _HASHTABLE__H_
