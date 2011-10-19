#ifndef _DS_TYPE_MAPPER__H_
#define _DS_TYPE_MAPPER__H_

#include <stdint.h>

#define DS_TYPE_MAPPER_TYPE_TO_ENUM(t, e) static int type_to_enum(const t *) { return e; } \
    static const char *type_to_str(const t *) { return (#e); }

#define DS_TYPE_MAPPER_ENUM_TO_STR(e, str) case e: return (#str);

struct ds_type_mapper
{
    enum
    {
        INT8,
        UINT8,
        INT16,
        UINT16,
        INT32,
        UINT32,
        INT64,
        UINT64,
        FLOAT,
        DOUBLE,
        BOOL
    };
    
    // mapping
    DS_TYPE_MAPPER_TYPE_TO_ENUM(int8_t, INT8);
    DS_TYPE_MAPPER_TYPE_TO_ENUM(uint8_t, UINT8);
    DS_TYPE_MAPPER_TYPE_TO_ENUM(int16_t, INT16);
    DS_TYPE_MAPPER_TYPE_TO_ENUM(uint16_t, UINT16);
    DS_TYPE_MAPPER_TYPE_TO_ENUM(int32_t, INT32);
    DS_TYPE_MAPPER_TYPE_TO_ENUM(uint32_t, UINT32);
    DS_TYPE_MAPPER_TYPE_TO_ENUM(int64_t, INT64);
    DS_TYPE_MAPPER_TYPE_TO_ENUM(uint64_t, UINT64);
    DS_TYPE_MAPPER_TYPE_TO_ENUM(float, FLOAT);
    DS_TYPE_MAPPER_TYPE_TO_ENUM(double, DOUBLE);
    DS_TYPE_MAPPER_TYPE_TO_ENUM(bool, BOOL);

    template<typename T>
    static int type_to_enum(const T *) { return -1; }
    
    template<typename T>
    static const char *type_to_str(const T *) { return "N/A"; }

    template<typename T>
    static bool is_same_type(const T *v, int t)
    {
        return type_to_enum(v) == t;
    }

    static const char *enum_to_str(int e)
    {
        switch(e)
        {
            DS_TYPE_MAPPER_ENUM_TO_STR(INT8, int8_t);
            DS_TYPE_MAPPER_ENUM_TO_STR(UINT8, uint8_t);
            DS_TYPE_MAPPER_ENUM_TO_STR(INT16, int16_t);
            DS_TYPE_MAPPER_ENUM_TO_STR(UINT16, uint16_t);
            DS_TYPE_MAPPER_ENUM_TO_STR(INT32, int32_t);
            DS_TYPE_MAPPER_ENUM_TO_STR(UINT32, uint32_t);
            DS_TYPE_MAPPER_ENUM_TO_STR(INT64, int64_t);
            DS_TYPE_MAPPER_ENUM_TO_STR(UINT64, uint64_t);
            DS_TYPE_MAPPER_ENUM_TO_STR(FLOAT, float);
            DS_TYPE_MAPPER_ENUM_TO_STR(DOUBLE, double);
            DS_TYPE_MAPPER_ENUM_TO_STR(BOOL, bool);
        }
        return "N/A";
    }

};

#endif // _DS_TYPE_MAPPER__H_
