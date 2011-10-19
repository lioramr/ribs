#ifndef _HASHTABLE_VECT__H_
#define _HASHTABLE_VECT__H_

#include <stdint.h>
#include "vmbuf.h"

template<typename T>
struct hashtable_vect
{
    struct Entry
    {
        uint32_t next; // must be first for the following tricks to work
        uint32_t key;
        uint32_t key_len;
        uint32_t val_index;
    };

    enum
    {
        DEFAULT_NUM_BUCKETS = 64
    };
    
    void init(uint32_t n = DEFAULT_NUM_BUCKETS);
    uint32_t bucket(const void *key, uint32_t n) const;

    T *insert(const void *key, uint32_t key_len, const T &val);
    T *insert(const char *key, const T &val);
    T *lookup(const void *key, uint32_t key_len) const;
    T *lookup(const char *key) const;

    T *begin() const { return (T *)vect.data(); }
    T *end() const { return (T *)vect.wloc(); }

    uint32_t vect_size() const { return size; }


    vmbuf buf;
    vmbuf vect;
    uint32_t last_element;
    uint32_t mask;
    uint32_t size;
};

template<typename T>
void hashtable_vect<T>::init(uint32_t n /* = DEFAULT_NUM_BUCKETS */)
{
    for (uint32_t m = (((uint32_t)-1) >> 1) + 1; m >= n; mask = m, m >>= 1);
    buf.init();
    buf.alloc(mask * sizeof(uint32_t)); // will always return 0, no need to store the ofs
    memset(buf.data(), 0, mask * sizeof(uint32_t));
    vect.init(sizeof(T) * mask);
    last_element = 0;
    --mask;
    size = 0;
}

template<typename T>
uint32_t hashtable_vect<T>::bucket(const void *key, uint32_t n) const
{
    register const unsigned char *p = (const unsigned char *)key;
    register const unsigned char *end = p + n;
    uint32_t h = 5381;
    for (; p != end; ++p)
        h = ((h << 5) + h) ^ *p;
    return h & mask;
}

template<typename T>
T *hashtable_vect<T>::insert(const void *key, uint32_t key_len, const T &val)
{
    uint32_t b = bucket(key, key_len);
    uint32_t ofs_key = buf.alloc(key_len);
    uint32_t ofs_entry = buf.alloc(sizeof(struct Entry));

    memcpy(buf.data() + ofs_key, key, key_len);
        
    struct Entry *e = (struct Entry *)(buf.data() + ofs_entry);
    e->key = ofs_key;
    e->key_len = key_len;
    e->val_index = last_element++;

    uint32_t *ofs_bucket_ptr = (((uint32_t *)buf.data()) + b);
    e->next = *ofs_bucket_ptr;
    *ofs_bucket_ptr = ofs_entry;
    T *t = vect.alloc<T>(); 
    *t = val;
    ++size;
    return t;
}

template<typename T>
T *hashtable_vect<T>::insert(const char *key, const T &val)
{
    return insert(key, strlen(key), val);
}

template<typename T>
T *hashtable_vect<T>::lookup(const void *key, uint32_t key_len) const
{
    uint32_t b = bucket(key, key_len);
    uint32_t ofs_entry = *(((uint32_t *)buf.data()) + b);
    while (ofs_entry > 0)
    {
        struct Entry *e = (struct Entry *)(buf.data() + ofs_entry);
        if (e->key_len == key_len && 0 == memcmp(buf.data() + e->key, key, key_len))
            return begin() + e->val_index;
        ofs_entry = e->next;
    }
    return NULL;
}

template<typename T>
T *hashtable_vect<T>::lookup(const char *key) const
{
    return lookup(key, strlen(key));
}

#endif // _HASHTABLE_VECT__H_
