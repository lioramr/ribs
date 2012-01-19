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
#ifndef _BITVECT__H_
#define _BITVECT__H_

struct bitvect
{
    void init()
    {
        vect.init();
    }

    void reset(uint32_t num_bits)
    {
        vect.reset();
        num_bits += 7;
        num_bits >>= 3; // num_bits is now num bytes
        vect.resize_if_less(num_bits);
        memset(vect.data(), 0, num_bits);
        vect.wseek(num_bits);
    }

    void set(uint32_t bitpos)
    {
        uint8_t bit = 1 << (bitpos & 7);
        bitpos >>= 3;
        *(vect.data() + bitpos) |= bit;
    }

    bool get(uint32_t bitpos)
    {
        uint8_t bit = 1 << (bitpos & 7);
        bitpos >>= 3;
        return (*(vect.data() + bitpos) & bit) != 0;
    }

    static void to_offsets(vmbuf *bits, vmbuf *result)
    {
        uint32_t loc = 0;
        for (char *p = bits->data(), *pend = bits->wloc(); p != pend; ++p, loc += 8)
        {
            uint8_t c = *p;
            uint8_t i = 0;
            while (c)
            {
                if (c & 1)
                {
                    *((uint32_t *)result->wloc()) = loc + i;
                    result->wseek(sizeof(uint32_t));
                }
                c >>= 1;
                ++i;
            }
        }
    }
    
    void to_offsets(vmbuf *result)
    {
        to_offsets(&vect, result);
    }
    
    vmbuf vect;
};

#endif // _BITVECT__H_
