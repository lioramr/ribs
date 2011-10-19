#ifndef _CLASS_FACTORY__H_
#define _CLASS_FACTORY__H_

template<typename T>
struct class_factory
{
    static T *create()
    {
        return new T;
    }
};

#endif // _CLASS_FACTORY__H_
