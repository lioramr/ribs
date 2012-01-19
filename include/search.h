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
    
    if (loc >= n)
        return *res = NULL, false;
    
    const T *ptr = data + loc;
    if (*ptr < v || v < *ptr)
        return *res = NULL, false;
    return *res = ptr, true;
}
    
#endif // _SEARCH__H_
