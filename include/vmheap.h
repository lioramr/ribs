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
#ifndef _VMHEAP__H_
#define _VMHEAP__H_

#include <unistd.h>
#include <stdint.h>
#include "vmbuf.h"

typedef uint32_t HEAP_HANDLE;

template<typename T>
class HeapDefaultPred
{
public:
    bool operator () (const T &item1, const T &item2) const
    {
        return item1 < item2;
    }
};

template<typename T>
class HeapDefaultDescPred
{
public:
    bool operator () (const T &item1, const T &item2) const
    {
        return item2 < item1;
    }
};


template <typename T, typename Pred = HeapDefaultPred<T> >
class vmheap
{
public:
    enum
    {
        DEFAULT_SIZE = 128
    };
    
    vmheap();

    void init(size_t nBaseSize = DEFAULT_SIZE);
		
    const T &top() const;
		
    void remove(HEAP_HANDLE handle);
 
    void removeItem(size_t index);

    bool validHandle(HEAP_HANDLE handle) const;

    const T &getItem(HEAP_HANDLE handle) const;

    const T &getItemAt(size_t index) const;

    const T &operator[](size_t index) const;

    void removeTop();
		
    HEAP_HANDLE insert(const T &v);

    bool empty() const;

    size_t size() const;

    size_t capacity() const;

    bool full() const;
 
private:
    typedef struct _TData
    {
        uint32_t key;
        T data;
    } TData;

    mutable vmbuf bufData;
    mutable vmbuf bufOfs;

    size_t maxSize;
    size_t numItems;
    Pred pred;

    void fixDown(size_t i);
 
    void build();
};

template <typename T, typename Pred>
inline vmheap<T, Pred>::vmheap()
{
}

template <typename T, typename Pred>
inline void vmheap<T, Pred>::init(size_t nBaseSize /* = HEAP_DEFAULT_SIZE */)
{
    numItems = 0;
    maxSize = nBaseSize;
    bufData.init();
    bufOfs.init();
    bufData.resize_if_less(nBaseSize * sizeof(TData));
    bufOfs.resize_if_less(nBaseSize * sizeof(uint32_t));
    TData *data = (TData *)bufData.data();
    uint32_t *ofs = (uint32_t *)bufOfs.data();

    for (size_t i = 0; i < nBaseSize; ++i)
    {
        ofs[i] = i;
        data[i].key = -1;
    }
}

template <typename T, typename Pred>
inline const T &vmheap<T, Pred>::top() const
{
    TData *data = (TData *)bufData.data();
    uint32_t *ofs = (uint32_t *)bufOfs.data();
    return data[ofs[0]].data;
}

template <typename T, typename Pred>
inline void vmheap<T, Pred>::remove(HEAP_HANDLE handle)
{
    TData *data = (TData *)bufData.data();
    removeItem(data[handle].key);
}

template <typename T, typename Pred> 
inline void vmheap<T, Pred>::removeItem(size_t index)
{
    if (index < numItems) 
    {
        --numItems;
        TData *data = (TData *)bufData.data();
        uint32_t *ofs = (uint32_t *)bufOfs.data();
        
        uint32_t o = ofs[index];
        data[o].key = -1;
        ofs[index] = ofs[numItems];
        data[ofs[index]].key = index;
        ofs[numItems] = o;
        fixDown(index);
    }
}

template <typename T, typename Pred>
inline bool vmheap<T, Pred>::validHandle(HEAP_HANDLE handle) const
{
    TData *data = (TData *)bufData.data();
    return ((~data[handle].key) != 0);
}

template <typename T, typename Pred>
inline const T &vmheap<T, Pred>::getItem(HEAP_HANDLE handle) const
{
    TData *data = (TData *)bufData.data();
    return data[handle].data;
}

template <typename T, typename Pred>
inline const T &vmheap<T, Pred>::getItemAt(size_t index) const
{
    TData *data = (TData *)bufData.data();
    uint32_t *ofs = (uint32_t *)bufOfs.data();
    return data[ofs[index]];
}

template <typename T, typename Pred>
inline const T &vmheap<T, Pred>::operator[](size_t index) const
{
    return getItem(index);
}

template <typename T, typename Pred>
inline void vmheap<T, Pred>::removeTop()
{
    if (numItems > 0)
    {
        --numItems;
        TData *data = (TData *)bufData.data();
        uint32_t *ofs = (uint32_t *)bufOfs.data();
        uint32_t first = ofs[0];
        uint32_t last = ofs[numItems];
        --data[first].key; // will set it to -1
        ofs[0] = last;
        ofs[numItems] = first;
        data[last].key = 0;
        fixDown(0);
    }
}

template <typename T, typename Pred> 
inline HEAP_HANDLE vmheap<T, Pred>::insert(const T &v)
{
    if (full())
    {
        size_t newSize = maxSize << 1;
        bufData.resize_by((newSize - maxSize) * sizeof(TData));
        bufOfs.resize_by((newSize - maxSize) * sizeof(uint32_t));
        TData *data = (TData *)bufData.data();
        uint32_t *ofs = (uint32_t *)bufOfs.data();
        for (size_t i = maxSize; i < newSize; ++i)
        {
            ofs[i] = i;
            data[i].key = -1;
        }
        maxSize = newSize;
    }
    uint32_t parent, pos;
    TData *data = (TData *)bufData.data();
    uint32_t *ofs = (uint32_t *)bufOfs.data();
    for (pos = numItems; pos > 0; pos = parent) 
    {
        parent = (pos - 1) >> 1;
        if (pred(data[ofs[parent]].data , v))
        {
            uint32_t ofsPos = ofs[pos];
            uint32_t ofsParent = ofs[parent];
            data[ofsPos].key = ofsParent;
            data[ofsParent].key = ofsPos;
            ofs[pos] = ofsParent;
            ofs[parent] = ofsPos;
        } else
            break;
    }
    TData *d = data + ofs[pos];
    d->data = v;
    d->key = pos;
    ++numItems;
    return ofs[pos];
}

template <typename T, typename Pred>
inline bool vmheap<T, Pred>::empty() const
{
    return (numItems == 0);
}

template <typename T, typename Pred>
inline size_t vmheap<T, Pred>::size() const
{
    return numItems;
}

template <typename T, typename Pred>
inline size_t vmheap<T, Pred>::capacity() const
{
    return maxSize;
}

template <typename T, typename Pred>
inline bool vmheap<T, Pred>::full() const
{
    return numItems >= maxSize;
}

template <typename T, typename Pred>
inline void vmheap<T, Pred>::fixDown(size_t i)
{
    TData *data = (TData *)bufData.data();
    uint32_t *ofs = (uint32_t *)bufOfs.data();
    uint32_t child;
    while ((child = (i << 1) + 1) < numItems) 
    {
        if (child + 1 < numItems && pred(data[ofs[child]].data , data[ofs[child+1]].data)) ++child;

        uint32_t parentOfs = ofs[i];
        uint32_t childOfs = ofs[child];
        
        if (pred(data[parentOfs].data , data[childOfs].data))
        {
            data[childOfs].key = parentOfs;
            data[parentOfs].key = childOfs;
            ofs[child] = parentOfs;
            ofs[i] = childOfs;
            i = child;
        }
        else 
            break;
    }
}

template <typename T, typename Pred>
inline void vmheap<T, Pred>::build()
{
    size_t lastParent = numItems >> 1;
    for (size_t i = lastParent; i != -1; --i)
        fixDown(i);
}

#endif // _VMHEAP__H_
