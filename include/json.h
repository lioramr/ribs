#ifndef _JSON__H_
#define _JSON__H_

#include <stdint.h>
#include "vmbuf.h"


struct json;
struct json_callback
{
    void null_callback(json *, const char *, const char *, const char *, const char *) {}
    void null_block_callback(json *, const char *, const char *) {}

    typedef void (json_callback::* callback)(json *, const char *, const char *, const char *, const char *);
    typedef void (json_callback::* block_callback)(json *, const char *, const char *);
};

#define JSON_CALLBACK(x) ((json_callback::callback)(&x))
#define JSON_BLOCK_CALLBACK(x) ((json_callback::block_callback)(&x))

struct json
{
    json() : callback_string(NULL),
             callback_primitive(NULL),
             callback_block_begin(NULL),
             callback_block_end(NULL) {};
    
    struct stack_item
    {
        const char *begin;
        const char *end;
        void set(const char *b, const char *e) { begin = b; end = e; }
        void reset() { begin = end = NULL; }
        bool isset() { return begin != end; }
    };
    int level;
    stack_item last_string;
    stack_item last_key;

    void reset_callbacks();
    int init();
    
    int parse_string();
    int parse_primitive();
    int parse(const char *js, void *cb = NULL);

    static void unescape_str(char *buf);
    
    void (json_callback::*callback_string)(json *json, const char *key_begin, const char *key_end, const char *val_begin, const char *val_end);
    void (json_callback::*callback_primitive)(json *json, const char *key_begin, const char *key_end, const char *val_begin, const char *val_end);

    void (json_callback::*callback_block_begin)(json *json, const char *key_begin, const char *key_end);
    void (json_callback::*callback_block_end)(json *json, const char *key_begin, const char *key_end);
  

    const char *cur;
    const char *err;
    json_callback *callback_object;
    vmbuf stack;
    
};

inline void json::reset_callbacks()
{
    callback_string = &json_callback::null_callback;
    callback_primitive = &json_callback::null_callback;
    callback_block_begin = &json_callback::null_block_callback;
    callback_block_end = &json_callback::null_block_callback;
}

inline int json::init()
{
    reset_callbacks();
    if (0 > stack.init())
    {
        err = "init";
        return -1;
    }
    return 0;
}

inline int json::parse_string()
{
    ++cur; // skip the starting \"
    const char *start = cur;
    for (; *cur; ++cur)
    {
        if ('\"' == *cur)
            break;
        else if ('\\' == *cur)
        {
            ++cur;
            if (0 == *cur) // error
                return -1;
        }
    }
    if ('\"' != *cur)
    {
        err = "string";
        return -1; // error
    }
    last_string.set(start, cur);
    (callback_object->*callback_string)(this, last_key.begin, last_key.end, start, cur);
    ++cur; // skip the ending \"
    return 0;
}

inline int json::parse_primitive()
{
    const char *start = cur;
    for (; *cur; ++cur)
    {
        switch (*cur)
        {
        case ']':
        case '}':
        case '\t':
        case '\r':
        case '\n':
        case ' ':
        case ',':
            (callback_object->*callback_primitive)(this, last_key.begin, last_key.end, start, cur);
            last_string.set(start, cur);
            return 0;
        }
    }
    err = "primitive";
    return -1;
}

inline int json::parse(const char *js, void *cb /* = NULL */)
{
    callback_object = (json_callback *)cb;
    last_key.reset();
    level = 0;
    cur = js;
    last_string.reset();
    for (; *cur; )
    {
        switch(*cur)
        {
        case '{':
        case '[':
            (callback_object->*callback_block_begin)(this, last_string.begin, last_string.end);
            last_key.reset();
            stack.copy(last_string);
            last_string.reset();
            ++level;
            break;
        case '}':
        case ']':
            if (level == 0)
            {
                err = "unbalanced parenthesis";
                return -1;
            }
            last_key.reset();
            stack.rollback(stack.wlocpos() - sizeof(struct stack_item));
            last_string = *(struct stack_item *)stack.wloc();
            (callback_object->*callback_block_end)(this, last_string.begin, last_string.end);
            --level;
            break;
        case '\"':
            if (0 > parse_string())
                return -1;
            continue;
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 't':
        case 'f':
        case 'n':
            if (0 > parse_primitive())
                return -1;
            continue;
        case ':':
            last_key = last_string;
            // no break here
        case '\t':
        case '\r':
        case '\n':
        case ' ':
            break;
        case ',':
            last_key.reset();
            break;
        default:
            break;
        }
        ++cur;
    }
    return 0;
}

/* static */
inline void json::unescape_str(char *buf)
{
    char table[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 
        0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 
        0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 
        0x60, 0x61, 0x08, 0x63, 0x64, 0x65, 0x0C, 0x67, 
        0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x0A, 0x6F, 
        0x70, 0x71, 0x0D, 0x73, 0x09, 0x75, 0x0B, 0x77, 
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 
        0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 
        0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 
        0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 
        0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 
        0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 
        0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
    };
    char *in = buf, *out = buf;
    for (; *in; )
    {
        switch (*in)
        {
        case '\\':
            ++in;
            *out++ = table[(uint8_t)(*in++)];
            break;
        default:
            *out++ = *in++;
        }
    }
    *out=0;
}

/*
inline void json_debug_print(const char *b, const char *e)
{
    size_t n = e - b;
    char buf[n + 1];
    memcpy(buf, b, n);
    buf[n] = 0;
    printf("%s", buf);
}
*/
/*
inline void json::callback_string(const char *key_begin, const char *key_end, const char *val_begin, const char *val_end)
{
    if (key_begin)
    {
        json_debug_print(key_begin, key_end);
        printf(" : ");
        json_debug_print(val_begin, val_end);
        printf("\n");
    }
}


inline void json::callback_primitive(const char *key_begin, const char *key_end, const char *val_begin, const char *val_end)
{
    if (key_begin)
    {
        json_debug_print(key_begin, key_end);
        printf(" : ");
        json_debug_print(val_begin, val_end);
        printf("\n");
    }
}

inline void json::callback_block_begin(const char *key_begin, const char *key_end)
{
    printf(">>> ");
    json_debug_print(key_begin, key_end);
    printf("\n");
}

inline void json::callback_block_end(const char *key_begin, const char *key_end)
{
    printf("<<< ");
    json_debug_print(key_begin, key_end);
    printf("\n");
}
*/


#endif // _JSON__H_
