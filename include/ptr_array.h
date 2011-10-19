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
