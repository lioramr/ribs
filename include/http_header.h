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
#ifndef _HTTP_HEADER__H_
#define _HTTP_HEADER__H_

#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "sstr.h"

/*
 * common http headers
 */
struct http_header_info
{
    char *referer;
    char *user_agent;
    char *cookie;
    char *x_forwarded_for;
    char *host;
    char *accept_encoding;
    char *content_type;
    char *if_none_match;
    char *accept_language;
    uint8_t accept_encoding_mask;
};

/*
 * HTTP Accept Encoding
 */
enum
{
    HTTP_AE_IDENTITY = 0x00,
    HTTP_AE_GZIP = 0x01,
    HTTP_AE_COMPRESS = 0x02,
    HTTP_AE_DEFLATE = 0x04,
    HTTP_AE_ALL = 0xFF
};

namespace http_header
{
    int init();
    int parse(char *headers, struct http_header_info *h);
    int decode_accept_encoding(struct http_header_info *h);
}

/*
 * inline
 */


#endif // _HTTP_HEADER__H_
