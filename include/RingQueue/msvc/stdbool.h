/* stdbool.h standard header */

#ifndef _STDBOOL
#define _STDBOOL

/* MSVC doesn't define _Bool or bool in C, but does have BOOL */
/* Note this doesn't pass autoconf's test because (bool) 0.5 != true */
#if defined(_MSC_VER) && (_MSC_VER < 1700)
#include <wtypes.h>
#include <windef.h>

typedef BOOL    _Bool;
#endif

#if (defined(_MSC_VER) && (_MSC_VER < 1700))

#define __bool_true_false_are_defined	1

#ifndef __cplusplus

#define bool	_Bool
#define false	0
#define true	1

#endif  /* __cplusplus */

#endif  /* _MSC_VER */

#endif  /* _STDBOOL */

/*
 * Copyright (c) 1992-2010 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V5.30:0009 */
