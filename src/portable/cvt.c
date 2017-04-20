/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */

#include "config.h"
#include <stdlib.h>

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#ifndef FLOAT_TYPE
#define FLOAT_TYPE double
#define FUNC_PREFIX
#define FLOAT_FMT_FLAG
#define FLOAT_NAME_EXT
#endif

#ifndef MAXDIG
#define MAXDIG 120
#endif

#ifndef APPEND
#define APPEND(a, b) APPEND2 (a, b)
#endif

#ifndef APPEND2
#define APPEND2(a, b) a##b
#endif

#ifndef FLOOR
#define FLOOR APPEND(floor, FLOAT_NAME_EXT)
#endif
#ifndef FABS
#define FABS APPEND(fabs, FLOAT_NAME_EXT)
#endif
#ifndef LOG10
#define LOG10 APPEND(log10, FLOAT_NAME_EXT)
#endif
#ifndef EXP
#define EXP APPEND(exp, FLOAT_NAME_EXT)
#endif
#ifndef ISINF
#define ISINF APPEND(isinf, FLOAT_NAME_EXT)
#endif
#ifndef ISNAN
#define ISNAN APPEND(isnan, FLOAT_NAME_EXT)
#endif

#ifndef EINVAL
#define EINVAL 22
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef __set_errno
#define __set_errno(x) errno = (x)
#endif

#define weak_extern2(name) 
weak_extern2 (FLOOR) weak_extern2 (LOG10) weak_extern2 (FABS)
weak_extern2 (EXP)

#ifndef HAVE_GCVT
char *
APPEND (FUNC_PREFIX, gcvt) (FLOAT_TYPE value, 
                            int ndigit, 
                            char *buf)
{
  sprintf (buf, "%.*" FLOAT_FMT_FLAG "g", ndigit, value);
  return buf;
}
#endif /* HAVE_GCVT */
