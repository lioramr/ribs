#ifndef _SSTR__H_
#define _SSTR__H_

#include <string.h>

#define SSTR(varname, val) \
    const char varname[] = val

#define SSTRE(varname, val) \
    extern const char varname[] = val

// local string (via static)
#define SSTRL(varname, val) \
    static const char varname[] = val

#define SSTREXTRN(varname) \
    extern const char varname[]

#define SSTRLEN(var) \
    (sizeof(var) - 1)

#define SSTRNCMP(var, with) \
    (strncmp(var, with, SSTRLEN(var)))

#define SSTRNCMPI(var, with) \
    (strncasecmp(var, with, SSTRLEN(var)))

#define SSTRCMP(var, with) \
    (strncmp(var, with, sizeof(var)))

#define SSTRISEMPTY(var) \
    ((var == NULL)||(var[0]=='\0'))

#define SSTRCMPI(var, with) \
    (strncasecmp(var, with, sizeof(var)))

#endif // _SSSTR__H_


