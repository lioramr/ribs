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
#ifndef _HTTP_COMMON__H_
#define _HTTP_COMMON__H_

#include "sstr.h"
#include "URI.h"

SSTREXTRN(HTTP_STATUS_200);

SSTREXTRN(HTTP_STATUS_300);
SSTREXTRN(HTTP_STATUS_301);
SSTREXTRN(HTTP_STATUS_302);
SSTREXTRN(HTTP_STATUS_303);
SSTREXTRN(HTTP_STATUS_304);

SSTREXTRN(HTTP_STATUS_400);
SSTREXTRN(HTTP_STATUS_403);
SSTREXTRN(HTTP_STATUS_404);
SSTREXTRN(HTTP_STATUS_411);

SSTREXTRN(HTTP_STATUS_500);
SSTREXTRN(HTTP_STATUS_501);
SSTREXTRN(HTTP_STATUS_503);

SSTREXTRN(HTTP_CONTENT_TYPE_TEXT_PLAIN);
SSTREXTRN(HTTP_CONTENT_TYPE_TEXT_XML);
SSTREXTRN(HTTP_CONTENT_TYPE_TEXT_HTML);
SSTREXTRN(HTTP_CONTENT_TYPE_JSON);
SSTREXTRN(HTTP_CONTENT_TYPE_IMAGE_GIF);

SSTREXTRN(HTTP_SET_COOKIE);

#endif // _HTTP_COMMON__H_


