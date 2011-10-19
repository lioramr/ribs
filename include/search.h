#ifndef _SEARCH__H_
#define _SEARCH__H_

#include <stdint.h>
#include <unistd.h>

class search
{
public:
    template<typename T>
    static uint32_t lower_bound(const T *data, uint32_t n, const T &v);

    template<typename T>
    static bool binary(const T *data, uint32_t n, const T &v, const T **res);
};


template<typename T>
inline /* static */ uint32_t search::lower_bound(const T *data, uint32_t n, const T &v)
{
    uint32_t l = 0, h = n;
    while (l < h) 
    {
        uint32_t m = (l + h) >> 1;
        if (data[m] < v)
        {
            l = m;
            ++l;
        } else
        {
            h = m;
        }
    }
    return l;
}

template<typename T>
inline /* static */ bool search::binary(const T *data, uint32_t n, const T &v, const T **res)
{
    uint32_t loc = search::lower_bound(data, n, v);
    const T *ptr = data + loc;
    if (*ptr < v || v < *ptr)
        return *res = NULL, false;
    return *res = ptr, true;
}
    
#endif // _SEARCH__H_
