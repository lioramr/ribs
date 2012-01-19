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
#ifndef _VMPOOL__H_
#define _VMPOOL__H_

#include <new>
#include "vmbuf.h"

template<typename U>
struct vmpool_op
{
    int (*init_func)(void *arg, uint32_t n);
    U *(*get_func)(void *arg);
    void (*put_func)(void *arg, U *);
    
    int init(uint32_t n) { return init_func(arg, n); }
    U *get() { return get_func(arg); }
    void put(U *e) { put_func(arg, e); }
    
    void *arg;
};

template<typename T>
struct vmpool
{
    int init(uint32_t max_num_elements);

    T *get();
    void put(T *e);

    static int static_init(void *arg, uint32_t n) { return ((vmpool<T> *)arg)->init(n); }
    template<typename U>
    static U *static_get(void *arg) { return ((vmpool<T> *)arg)->get(); }
    template<typename U>
    static void static_put(void *arg, U *e) { ((vmpool<T> *)arg)->put((T *)e); }

    template<typename U>
    vmpool_op<U> get_op() { return (vmpool_op<U>){ static_init, static_get<U>, static_put<U>, this }; }
    
    vmbuf elements;
    vmbuf pool;
};

template<typename T>
inline int vmpool<T>::init(uint32_t max_num_elements)
{
    if (0 > elements.init(max_num_elements * sizeof(T)) ||
        0 > pool.init(max_num_elements * sizeof(T *)))
        return -1;
    // hot pages are last, page faults will occurs from end to start
    // for (T *p = (T *)elements.data(), *end = p + max_num_elements; p != end; ++p)
    
    // hot pages are first, page faults will occurs from start to end
    for (T *p = (T *)elements.data() + max_num_elements - 1, *end = (T *)elements.data() - 1; p != end; --p)
        put(p);
    return 0;
}

template<typename T>
inline T *vmpool<T>::get()
{
    pool.wrewind(sizeof(T *));
    T *t = *(T **)pool.wloc();
    T::init(t);
    // printf("get %p %p\n", this, t);
    return t;
}

template<typename T>
inline void vmpool<T>::put(T *e)
{
    // printf("put %p %p\n", this, e);
    *(T **)pool.wloc() = e;
    pool.unsafe_wseek(sizeof(T *));
}

#endif // _VMPOOL__H_
