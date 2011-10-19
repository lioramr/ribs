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
