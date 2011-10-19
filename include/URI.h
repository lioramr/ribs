#ifndef _URI__H_
#define _URI__H_

#include "hashtable.h"

namespace URI
{
    inline char hex_val_char(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        else if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return 0;
    }
    
    inline char hex_val(char **h)
    {
        char *s = *h;
        char val = 0;
        if (*s)
        {
            val = hex_val_char(*s) << 4;
            ++s;
            if (*s)
                val += hex_val_char(*s++);
        }
        *h = s;
        return val;
    }
    
    inline void decode(char *uri)
    {
        char *p1 = uri, *p2 = p1;
        const char SPACE = ' ';

        while (*p1)
        {
            switch (*p1)
            {
            case '%':
                ++p1; // skip the %
                *p2++ = hex_val(&p1);
                break;
            case '+':
                *p2++ = SPACE;
                ++p1;
                break;
            default:
                *p2++ = *p1++;
            }
        }
        *p2 = 0;
    }

    
    /* generates the bits table used in the encode function below */
    inline int __offline_use_only__generate_encode_bits_table()
    {
        int b;
        uint8_t bits[32];
        memset(bits, 0, sizeof(bits));
        for (int i = 0; i < 256; ++i)
        {
            b = 0;
            switch(i)
            {
            case '.':
            case '-':
            case '*':
            case '_':
            b = 1;
            default:
                if (!(b ||
                      (i >= 'a' && i <= 'z') ||
                      (i >= 'A' && i <= 'Z') ||
                      (i >= '0' && i <= '9')
                      ))
                {
                    bits[i >> 3] |= 1 << (i & 7);
                }
            }
        }

        for (int i = 0; i < 32; ++i)
        {
            printf("0x%02X, ", (int)bits[i]);
            if (((i + 1) & 7) == 0)
                printf("\n");
        }
        return 0;
    }

    inline char* encode(const char *in, char *out)
    {
        static uint8_t bits[] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x9B, 0x00, 0xFC, 
            0x01, 0x00, 0x00, 0x78, 0x01, 0x00, 0x00, 0xF8, 
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
        };
        static char hex_chars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                    'A', 'B', 'C', 'D', 'E', 'F' };
        const uint8_t *inbuf = (const uint8_t *)in;
        uint8_t *outbuf = (uint8_t *)out;
        for ( ; *inbuf; ++inbuf)
        {
            register uint8_t c = *inbuf;
            if (c == ' ')
            {
                *outbuf++ = '+';
            } else if ((bits[c >> 3] & (1 << (c & 7))) > 0)
            {
                *outbuf++ = '%';
                *outbuf++ = hex_chars[c >> 4];
                *outbuf++ = hex_chars[c & 15];
            }
            else
                *outbuf++ = c;
        }
        *outbuf = 0;
        return (char *)outbuf;
    }

    inline char* purify_url(const char *in, char *out)
    {
        static char hex_chars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                    'A', 'B', 'C', 'D', 'E', 'F' };
        const uint8_t *inbuf = (const uint8_t *)in;
        uint8_t *outbuf = (uint8_t *)out;
        for ( ; *inbuf; ++inbuf)
        {
            register uint8_t c = *inbuf;
            if (c <= 31)
            {
                *outbuf++ = '%';
                *outbuf++ = hex_chars[c >> 4];
                *outbuf++ = hex_chars[c & 15];
            }
            else
                *outbuf++ = c;
        }
        *outbuf = 0;
        return (char *)outbuf;
    }

    inline int decode(char *query, int (*callback)(char *name, char *value, void *arg), void *arg)
    {
        int n = 0;
        while (*query)
        {
            char *p = strchrnul(query, '&');
            if (*p)
            {
                *p = 0;
                ++p;
            }
        
            char *p2 = strchrnul(query, '=');
            if (*p2)
                *p2++ = 0;

            decode(p2);

            if (0 > callback(query, p2, arg))
                break;
        
            query = p;
        }
        return n;
    }

    inline int decode(char *query, hashtable *ht)
    {
        int n = 0;
        while (*query)
        {
            int n = 0;
            char mods[2];
            char *mods_pos[2];
            
            char *p = strchrnul(query, '&');
            if (*p)
            {
                mods[n] = *p;
                mods_pos[n] = p;
                ++n;
                *p++ = 0;
            }
        
            char *p2 = strchrnul(query, '=');
            if (*p2)
            {
                mods[n] = *p2;
                mods_pos[n] = p2;
                ++n;
                *p2++ = 0;
            }

            if (!ht->is_found(ht->lookup(query)))
                decode(ht->get_val(ht->insert(query, p2)));
            // restore chars
            for (char **pos = mods_pos, **pos_end = pos + n, *chars = mods; pos != pos_end; ++pos, ++chars)
                **pos = *chars;
            query = p;
        }
        return n;
    }
}

#endif // _URI__H_
