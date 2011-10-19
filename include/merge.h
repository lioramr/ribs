#ifndef _MERGE__H_
#define _MERGE__H_
#include "heap.h"
#include "vmbuf.h"

struct sorted_vect
{
    void init(uint32_t *data, uint32_t n);

    void seek_next() { ++current; }
    bool has_more() { return current != end; }
    uint32_t get_current() { return *current; }
    size_t size() { return end - current; }

    uint32_t *current;
    uint32_t *end;
};

inline void sorted_vect::init(uint32_t *data, uint32_t n)
{
    current = data;
    end = data + n;
}


struct merge_union
{
    void init(sorted_vect *l, size_t n)
    {
        lists = l;
        size = n;
        h.init(size);
        for (size_t i = 0; i < n; ++i, ++l)
            h.add(l->get_current(), i);
    }
    
    void merge(vmbuf *result)
    {
        h.build();
        while (!h.empty())
        {
            uint32_t current = h.top_key();
            *((uint32_t *)result->wloc()) = current;
            result->wseek(sizeof(uint32_t));
            while (!h.empty() && h.top_key() == current)
            {
                uint32_t loc = h.pop();
                sorted_vect *v = lists + loc;
                v->seek_next();
                if (v->has_more())
                    h.push(v->get_current(), loc);
            }
        }
    }

    sorted_vect *lists;
    heap<uint32_t, uint32_t> h;
    size_t size;
};

struct union_container : merge_union
{
    uint32_t get_current()
    {
        return h.top_key();
    }

    bool has_more()
    {
        return !h.empty();
    }

    void seek_next()
    {
        uint32_t current = h.top_key();
        while (!h.empty() && h.top_key() == current)
        {
            uint32_t loc = h.pop();
            sorted_vect *v = lists + loc;
            v->seek_next();
            if (v->has_more())
                h.push(v->get_current(), loc);
        }
    }
};

struct merge_intersection
{
    static void intersect(sorted_vect *vects, uint16_t num_vects, vmbuf *result)
    {
        sorted_vect *vects_end = vects + num_vects;
        // find the list with the highest value at the top
        sorted_vect *vect = vects;
        uint32_t top = vect->get_current();
        for (sorted_vect *v = vects + 1; v != vects_end; ++v)
        {
            uint32_t t = v->get_current();
            if (top < t)
            {
                vect = v;
                top = t;
            }
        }
        
        while (1)
        {
            uint32_t current = vect->get_current();
            uint16_t unmatched_vects = num_vects;
            for (sorted_vect *v = vects; v != vects_end; ++v)
            {
                while (v->has_more() && v->get_current() < current)
                    v->seek_next();
                
                if (!v->has_more())
                    return;
                
                if (v->get_current() != current)
                {
                    vect = v; // switch to vector with higher top
                    break;
                }
                
                v->seek_next();
                --unmatched_vects;
            }
            if (0 == unmatched_vects)
            {
                *((uint32_t *)result->wloc()) = current;
                result->wseek(sizeof(uint32_t));
            }
        }
    }


    static void intersect(union_container *vects, uint16_t num_vects, vmbuf *result)
    {
        union_container *vects_end = vects + num_vects;
        // find the list with the highest value at the top
        union_container *vect = vects;
	if (!vect->has_more())
            return;
        uint32_t top = vect->get_current();
        for (union_container *v = vects + 1; v != vects_end; ++v)
        {
            if (!v->has_more())
		return;
            uint32_t t = v->get_current();
            if (top < t)
            {
                vect = v;
                top = t;
            }
        }
        
        while (1)
        {
            uint32_t current = vect->get_current();
            uint16_t unmatched_vects = num_vects;
            for (union_container *v = vects; v != vects_end; ++v)
            {
                while (v->has_more() && v->get_current() < current)
                    v->seek_next();
                
                if (!v->has_more())
                    return;
                
                if (v->get_current() != current)
                {
                    vect = v; // switch to vector with higher top
                    break;
                }
                
                v->seek_next();
                --unmatched_vects;
            }
            if (0 == unmatched_vects)
            {
                *((uint32_t *)result->wloc()) = current;
                result->wseek(sizeof(uint32_t));
            }
        }
    }

    static void merge_bits(sorted_vect *vects, uint16_t num_vects, uint32_t total_num_records, vmbuf *result)
    {
        uint32_t num_bytes = (total_num_records + 7) >> 3;
        result->alloczero(num_bytes);
        char *res = result->data();
        sorted_vect *vect = vects;
        sorted_vect *vect_end = vect + num_vects;
        for (; vect != vect_end; ++vect)
        {
            while (vect->has_more())
            {
                uint32_t loc = vect->get_current();
                uint8_t bit = 1 << (loc & 7);
                loc >>= 3;
                *(res + loc) |= bit;
                vect->seek_next();
            }
        }
    }

    static void bit_vect_to_pos(vmbuf *vect, vmbuf *result)
    {
        uint32_t loc = 0;
        for (char *p = vect->data(), *pend = vect->wloc(); p != pend; ++p, loc += 8)
        {
            uint8_t c = *p;
            uint8_t i = 0;
            while (c)
            {
                if (c & 1)
                {
                    *((uint32_t *)result->wloc()) = loc + i;
                    result->wseek(sizeof(uint32_t));
                }
                c >>= 1;
                ++i;
            }
        }
    }

    static void merge_bits_and(vmbuf *vect1, vmbuf *vect2, vmbuf *res)
    {
        vmbuf *bitvect = vect1;
        if (bitvect->wlocpos() > 0)
        {
            if (vect2->wlocpos() > 0)
            {
                for (char *p = bitvect->data(), *pend = bitvect->wloc(), *other = vect2->data(); p != pend; ++p, ++other)
                {
                    *p &= *other;
                }
            }
        } else
            bitvect = vect2;
        bit_vect_to_pos(bitvect, res);
    }


    


    static uint32_t *begin(vmbuf *result)
    {
        return (uint32_t*)result->data();
    }
    
    static size_t size(vmbuf *result)
    {
        return result->wlocpos() / sizeof(uint32_t);
    }

    static void dump(vmbuf *result)
    {
        uint32_t *b = begin(result);
        uint32_t *e = b + size(result);
        for (uint32_t *p = b; p != e; ++p)
            printf("%u\n", *p);
    }
    
};


#endif // _MERGE__H_
