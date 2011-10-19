#ifndef _HEAP__H_
#define _HEAP__H_

#include "vmbuf.h"
#include <stdint.h>

template<typename K, typename V>
class heap
{
public:
    typedef struct entry
    {
        K key;
        V val;
    } entry_t;
    
    void init(uint32_t num_items);
    void build();
    entry_t *get_entries();
    void fix_down(uint32_t parent);
    V pop();
    void push(const K & k, const V &v);
    void add(const K & k, const V &v);
    V top() { return get_entries()->val; }
    K top_key() { return get_entries()->key; }
    bool empty() { return num_entries == 0; }

    vmbuf buf_entries;
    uint32_t num_entries;
};


template<typename K, typename V>
inline void heap<K, V>::init(uint32_t num_items)
{
    buf_entries.init(sizeof(entry_t) * num_items);
    num_entries = 0;
}

template<typename K, typename V>
inline void heap<K, V>::build()
{
    uint32_t last = num_entries >> 1;
    for (int i = last; i >= 0; --i)
        fix_down(i);
}

template<typename K, typename V>
inline typename heap<K, V>::entry_t *heap<K, V>::get_entries()
{
    return (entry_t *)buf_entries.data();
}

template<typename K, typename V>
inline void heap<K, V>::fix_down(uint32_t parent)
{
    entry_t *entries = get_entries();
    uint32_t child;
    while ((child = (parent << 1) + 1) < num_entries) 
    {
        entry_t *c = entries + child;
        entry_t *p = entries + parent;
        K k0 = c->key;
        if (child + 1 < num_entries)
        {
            K k1 = (c+1)->key;
            if (k0 > k1)
            {
                ++c;
                ++child;
                k0 = k1;
            }
        }
        if (p->key > k0)
        {
            entry_t t = *p;
            *p = *c;
            *c = t;
            parent = child;
        }
        else 
            break;
    }
}

template<typename K, typename V>
inline V heap<K, V>::pop()
{
    entry_t *entries = get_entries();
    entry_t e = entries[0];
    --num_entries;
    entries[0] = entries[num_entries]; // move last to top
    fix_down(0); // fix the heap
    return e.val;
}

template<typename K, typename V>
inline void heap<K, V>::push(const K & k, const V &v)
{
    entry_t *entries = get_entries();
    uint32_t parent, insert_pos;
    for (insert_pos = num_entries; insert_pos > 0; insert_pos = parent)
    {
        parent = (insert_pos - 1) >> 1;
        entry_t *p = entries + parent;
        if (p->key > k)
        {
            entry_t tmp = *p;
            entry_t *l = entries + insert_pos;
            *p = *l;
            *l = tmp;
        } else
            break;
    }
    entries[insert_pos] = (entry_t){ k, v };
    ++num_entries;
}

template<typename K, typename V>
inline void heap<K, V>::add(const K & k, const V &v)
{
    entry_t *entries = get_entries();
    entries[num_entries++] = (entry_t){ k, v };
}

#endif // _HEAP__H_
