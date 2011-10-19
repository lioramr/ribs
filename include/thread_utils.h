#ifndef _THREAD_UTILS__H_
#define _THREAD_UTILS__H_

#define STATIC_THREAD_VAR(type, var)  \
    static __thread type *var = NULL; \
    if (NULL == var) var = new type

#endif // _THREAD_UTILS__H_
