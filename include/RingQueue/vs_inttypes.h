
#ifndef _JIMI_INTTYPES_H_
#define _JIMI_INTTYPES_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1700)
#include "msvc/inttypes.h"
#else
#include <inttypes.h>
#endif  /* _MSC_VER */

#endif  /* _JIMI_INTTYPES_H_ */
