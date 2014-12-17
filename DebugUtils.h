
#ifndef _JIMI_UTIL_DEBUGUTILS_H_
#define _JIMI_UTIL_DEBUGUTILS_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "jimi/core/jimi_namespace.h"
#include "jimi/internal/NonCopyable.h"
#include "jimi/util/DumpUtils.h"

NS_JIMI_BEGIN

class DebugUtils : public internal::NonCopyable
{
public:
    DebugUtils() {};
    ~DebugUtils() {};

public:
    static void dump(void *p, size_t size, bool alignedTo = false,
                     unsigned int alignment = 16,
                     unsigned int extraHead = 0,
                     unsigned int extraTail = 0);

    static void dump2(void *p, size_t size, bool alignedTo = false,
                      unsigned int alignment = 16,
                      unsigned int extraHead = 0,
                      unsigned int extraTail = 0);
};

void DebugUtils::dump(void *p, size_t size, bool alignedTo /* = false */,
                      unsigned int alignment /* = 16 */,
                      unsigned int extraHead /* = 0 */,
                      unsigned int extraTail /* = 0 */)
{
#if defined(_DEBUG) && !defined(NDEBUG)
    __try {
        DumpUtils::dump(p, size, alignedTo, alignment, extraHead, extraTail);
    }
    __except(1) {
        // Do nothing!!!
        printf(" Read the memory failed.\n\n");
    }
#endif
}

void DebugUtils::dump2(void *p, size_t size, bool alignedTo /* = false */,
                       unsigned int alignment /* = 16 */,
                       unsigned int extraHead /* = 0 */,
                       unsigned int extraTail /* = 0 */)
{
#if defined(_DEBUG) && !defined(NDEBUG)
    __try {
        DumpUtils::dump2(p, size, alignedTo, alignment, extraHead, extraTail);
    }
    __except(1) {
        // Do nothing!!!
        printf(" Read the memory failed.\n\n");
    }
#endif
}

NS_JIMI_END

#endif  /* _JIMI_UTIL_DEBUGUTILS_H_ */
