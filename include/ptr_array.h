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
#ifndef _PTR_ARRAY__H_
#define _PTR_ARRAY__H_

template<typename T>
struct ptr_array
{
    ptr_array() : array(NULL) {}
    
    void free()
    {
        if (NULL != array)
        {
            for (T **t = (T **)array, **tend = t + size; t != tend; ++t)
                delete *t;
            delete[] array;
            array = NULL;
            size = 0;
        }
    }
    
    template<typename F>
    void init(size_t n, const F &factory)
    {
        this->free();
        size = n;
        array = new T*[size]; // array of pointers
        for (T **a = array, **a_end = a + size; a != a_end; ++a)
            *a = factory();
    }

    T *get(size_t index) { return (T *)array[index]; }
    T **begin() const { return array; }
    T **end() const { return array + size; }
    
    T **array;
    size_t size;
};

#endif // _PTR_ARRAY__H_
