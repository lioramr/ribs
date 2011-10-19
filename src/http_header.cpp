#include "http_header.h"
#include "hashtable.h"
#include <stddef.h>


struct request_headers
{
    const char *name;
    size_t ofs;
};

static struct request_headers request_headers[] = {
    { "referer",         offsetof(struct http_header_info, referer)         },
    { "user-agent",      offsetof(struct http_header_info, user_agent)      },
    { "cookie",          offsetof(struct http_header_info, cookie)          },
    { "x-forwarded-for", offsetof(struct http_header_info, x_forwarded_for) },
    { "host",            offsetof(struct http_header_info, host)            },
    { "accept-encoding", offsetof(struct http_header_info, accept_encoding) },
    { "content-type",    offsetof(struct http_header_info, content_type)    },
    { "if-none-match",   offsetof(struct http_header_info, if_none_match)   },
    { "accept-language", offsetof(struct http_header_info, accept_language) },
    /* terminate the list */
    { NULL, 0 }
};

static hashtable ht_request_headers;

static int parse_callback(char *name, char *value, void *arg)
{
    // lower case name first
    for (char *p = name; *p; *p = tolower(*p), ++p);
    uint32_t *ofs = ht_request_headers.lookup32(name, strlen(name));
    if (NULL != ofs)
    {
        *(char **)((char *)arg + *ofs) = value;
    }
    return 0;
}

static int parse(char *headers, int (*callback)(char *name, char *value, void *arg), void *arg)
{
    int n = -1;
    while (*headers)
    {
        char *p = strchrnul(headers, '\r'); // find the end
        if (*p) *p++ = 0;
        if (*p == '\n') ++p;
        
        char *p2 = strchrnul(headers, ':');
        if (*p2) *p2++ = 0;
        if (*p2 == ' ') *p2++ = 0; // skip the space
        
        if (0 > callback(headers, p2, arg))
            break;
        ++n;
        headers = p; // proceed to next pair
    }
    return n;
}

/* static */
int http_header::init()
{
    ht_request_headers.init(16);
    struct request_headers *rh = request_headers;
    for (; rh->name; ++rh)
    {
        ht_request_headers.insert32(rh->name, strlen(rh->name), rh->ofs);
    }
    return 0;
}

int http_header::parse(char *headers, struct http_header_info *h)
{
    static char no_value[] = { '-', 0 };
    *h = (struct http_header_info) {
        no_value,
        no_value,
        no_value,
        no_value,
        no_value,
        no_value,
        no_value,
        no_value,
        no_value,
        HTTP_AE_IDENTITY
    };
    return ::parse(headers, parse_callback, h);
}

int http_header::decode_accept_encoding(struct http_header_info *h)
{
    static const char QVAL[] = "q=";
    static const char GZIP[] = "gzip";
    static const char COMPRESS[] = "compress";
    static const char DEFLATE[] = "deflate";
    char *p = h->accept_encoding;
    if (*p == '-') // wasn't specified
    {
        h->accept_encoding_mask = HTTP_AE_IDENTITY;
    } else if (*p == '*') // accept all
    {
        h->accept_encoding_mask = HTTP_AE_ALL;
    } else
    {
        while (*p)
        {
            char *val = p;
            p = strchrnul(p, ',');
            if (*p)
                *p++ = 0;
            for (; *p == ' '; *p++ = '\0');
                
            float qval = 1.0;
            char *qvalstr = strchrnul(val, ';');
            if (*qvalstr)
            {
                *qvalstr++ = 0;
                for (; *qvalstr == ' '; *qvalstr++ = '\0');
                if (0 == SSTRNCMP(QVAL, qvalstr))
                {
                    qval = atof(qvalstr + SSTRLEN(QVAL));
                }
            } 
            if (qval > 0.0001)
            {
                if (*val == '*')
                    h->accept_encoding_mask |= HTTP_AE_ALL;
                else if (0 == SSTRNCMP(GZIP, val))
                    h->accept_encoding_mask |= HTTP_AE_GZIP;
                else if (0 == SSTRNCMP(COMPRESS, val))
                    h->accept_encoding_mask |= HTTP_AE_COMPRESS;
                else if (0 == SSTRNCMP(DEFLATE, val))
                    h->accept_encoding_mask |= HTTP_AE_DEFLATE;
            }
        }
    }
    return 0;
}

