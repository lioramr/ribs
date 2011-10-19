#ifndef _MIME_TYPES__H_
#define _MIME_TYPES__H_

#include "hashtable.h"

struct mime_types
{
private:
    mime_types();
    mime_types(const struct mime_types &);
    mime_types &operator =(const struct mime_types &);
    ~mime_types();
    
public:
    static struct mime_types *instance();
    void load();
    const char *type(const char *ext);
    const char *mime_type(const char *file_name);

    hashtable htMimes;
    
};

#endif // _MIME_TYPES__H_
