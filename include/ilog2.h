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
#ifndef _ILOG2__H_
#define _ILOG2__H_

inline unsigned int ilog2(register unsigned int x)
{
    register unsigned int l=0;
    if (x >= 1<<16) { x>>=16; l|=16; }
    if (x >= 1<<8)  { x>>=8;  l|=8;  }
    if (x >= 1<<4)  { x>>=4;  l|=4;  }
    if (x >= 1<<2)  { x>>=2;  l|=2;  }
    if (x >= 1<<1) l|=1;
    return l;
}

#endif // _ILOG2__H_
