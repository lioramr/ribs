#ifndef _SEQUENCER__H_
#define _SEQUENCER__H_

template<typename T>
struct DefaultHash
{
    uint32_t get_hash(const T &a)
    {
        return a;
    }
};

template<typename KEY, typename HASH_FUNC=DefaultHash<KEY> >
class sequencer
{
public:
    enum
    {
        DEFAULT_NUM_BUCKETS = 64
    };
    typedef KEY key_t;
    typedef uint16_t value_t;

    struct Entry
    {
        uint32_t next; // must be first for the following tricks to work
        key_t key;
        value_t val;
    };

    HASH_FUNC hash_func;
    
    void init(uint32_t n = DEFAULT_NUM_BUCKETS);
    uint32_t bucket(const key_t &key);

    value_t insert(const key_t &key);
    value_t lookup(const key_t &key);
    value_t get(const key_t &key);

    vmbuf buf;
    uint32_t mask;
    uint32_t size;
    value_t current;
};


/*
 * inline functions
 */

template<typename KEY, typename HASH_FUNC>
inline void sequencer<KEY, HASH_FUNC>::init(uint32_t n /* = DEFAULT_NUM_BUCKETS */)
{
    for (uint32_t m = (((uint32_t)-1) >> 1) + 1; m >= n; mask = m, m >>= 1);
    buf.init();
    buf.alloc(mask * sizeof(uint32_t)); // will always return 0, no need to store the ofs
    memset(buf.data(), 0, mask * sizeof(uint32_t));
    --mask;
    size = 0;
    current = 1; // 0 is special case
}

template<typename KEY, typename HASH_FUNC>
inline uint32_t sequencer<KEY, HASH_FUNC>::bucket(const key_t &key)
{
    return hash_func.get_hash(key) & mask;
}

template<typename KEY, typename HASH_FUNC>
inline typename sequencer<KEY, HASH_FUNC>::value_t sequencer<KEY, HASH_FUNC>::insert(const key_t &key)
{
    value_t val = current++;

    uint32_t b = bucket(key);
    uint32_t ofs_entry = buf.alloc(sizeof(struct Entry));
    
    struct Entry *e = (struct Entry *)(buf.data() + ofs_entry);
    e->key = key;
    e->val = val;

    uint32_t *ofs_bucket_ptr = (((uint32_t *)buf.data()) + b);
    e->next = *ofs_bucket_ptr;
    *ofs_bucket_ptr = ofs_entry;
    ++size;
    return val;
}

template<typename KEY, typename HASH_FUNC>
inline typename sequencer<KEY, HASH_FUNC>::value_t sequencer<KEY, HASH_FUNC>::lookup(const key_t &key)
{
    uint32_t b = bucket(key);
    uint32_t ofs_entry = *(((uint32_t *)buf.data()) + b);
    while (ofs_entry > 0)
    {
        struct Entry *e = (struct Entry *)(buf.data() + ofs_entry);
        if (e->key == key)
            return e->val;
        ofs_entry = e->next;
    }
    return 0;
}

template<typename KEY, typename HASH_FUNC>
inline typename sequencer<KEY, HASH_FUNC>::value_t sequencer<KEY, HASH_FUNC>::get(const key_t &key)
{
    value_t v = lookup(key);
    if (0 < v)
        return v;
    return insert(key);
}

#endif // _SEQUENCER__H_
