
#ifndef _JIMI_UTIL_DUMPUTILS_H_
#define _JIMI_UTIL_DUMPUTILS_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "jimi/core/jimi_def.h"
//#include "jimi/core/jimi_namespace.h"
//#include "jimi/core/jimi_config.h"
#include "jimi/internal/NonCopyable.h"

#include <stdio.h>

NS_JIMI_BEGIN

class DumpUtils : public NonCopyable
{
public:
#if defined(JIMI_HAS_DEFAULTED_FUNCTIONS) && (JIMI_HAS_DEFAULTED_FUNCTIONS != 0)
    //! Allow default construction
    JIMI_CONSTEXPR DumpUtils() = default;
    ~DumpUtils() = default;
#else
    //! Allow default construction
    DumpUtils() {}
    ~DumpUtils() {}
#endif

public:
    static void dump(void *p, size_t size, bool alignedTo = false,
                     unsigned int alignment = 16,
                     unsigned int extraHead = 0, unsigned int extraTail = 0);

    static void dump2(void *p, size_t size,  bool alignedTo = false,
                      unsigned int alignment = 16,
                      unsigned int extraHead = 0, unsigned int extraTail = 0);
};

void DumpUtils::dump(void *p, size_t size, bool alignedTo /* = false */,
                     unsigned int alignment /* = 16 */,
                     unsigned int extraHead /* = 0 */,
                     unsigned int extraTail /* = 0 */)
{
    // 输出内存dump
    size_t i, j;
    size_t addr;
    size_t lineTotal;
    unsigned char *cur;
    void *startAddr, *endAddr;

    jimi_assert(p != NULL);
    //jimi_assert(p != (void *)0xCCCCCCCC);

    // Whether the start address aligned to alignment?
    if (alignedTo) {
        startAddr = (void *)JIMI_ADDR_ALIGNED_DOWNTO(p, alignment);
        endAddr   = (void *)JIMI_ADDR_ALIGNED_TO((size_t)p + size, alignment);
    }
    else {
        startAddr = p;
        endAddr   = (void *)((size_t)p + size);
    }

    // Extend the extra lines at head or tail
    if (extraHead > 0)
        startAddr = JIMI_PVOID_SUB(startAddr, alignment * extraHead);
    if (extraTail > 0)
        endAddr   = JIMI_PVOID_ADD(endAddr, alignment * extraTail);

    // Get the total of display lines
    lineTotal = ((size_t)endAddr - (size_t)startAddr + (alignment - 1)) / alignment;

    printf("--------------------------------------------------------------\n");
    printf("  Addr = 0x%08X, Size = %u bytes\n", (unsigned int)p, (unsigned int)size);
    printf("--------------------------------------------------------------\n");
    printf("\n");

    if (p == NULL || p == (void *)0xCCCCCCCC) {
        printf("  Can not read the data from the address.\n");
        printf("\n");
        return;
    }

    printf("  Address    0  1  2  3  4  5  6  7 |  8  9  A  B  C  D  E  F\n");
    printf("--------------------------------------------------------------\n");
    printf("\n");

    addr = (size_t)startAddr;
    for (i = 0; i < lineTotal; ++i) {
        // display format preview
        //printf("  %08X  00 01 02 03 04 05 06 07   08 09 0A 0B 0C 0D 0E 0F\n", (unsigned int)addr);
        printf("  %08X  ", (unsigned int)addr);
        cur = (unsigned char *)addr;
        for (j = 0; j < alignment; ++j) {
            printf("%02X", (unsigned int)(*cur));
            if (j < alignment - 1) {
                if ((j & 7) != 7)
                    printf(" ");
                else
                    printf("   ");
            }
            cur++;
        }
        printf("    ");
        cur = (unsigned char *)addr;
        for (j = 0; j < alignment; ++j) {
            if (*cur >= 128)
                printf("?");
            else if (*cur >= 32)
                printf("%c", (unsigned char)(*cur));
            else
                printf(".");
            cur++;
        }
        printf("\n");
        addr += alignment;
    }

    printf("\n");
}

void DumpUtils::dump2(void *p, size_t size, bool alignedTo /* = false */,
                      unsigned int alignment /* = 16 */,
                      unsigned int extraHead /* = 0 */,
                      unsigned int extraTail /* = 0 */)
{
    // 输出内存dump
    size_t i, j;
    size_t addr;
    size_t lineTotal;
    unsigned char *cur;
    void *startAddr, *endAddr;

    jimi_assert(p != NULL);
    //jimi_assert(p != (void *)0xCCCCCCCC);

    // Whether the start address aligned to alignment?
    if (alignedTo) {
        startAddr = (void *)JIMI_ADDR_ALIGNED_DOWNTO(p, alignment);
        endAddr   = (void *)JIMI_ADDR_ALIGNED_TO((size_t)p + size, alignment);
    }
    else {
        startAddr = p;
        endAddr   = (void *)((size_t)p + size);
    }

    // Extend the extra lines at head or tail
    if (extraHead > 0)
        startAddr = JIMI_PVOID_SUB(startAddr, alignment * extraHead);
    if (extraTail > 0)
        endAddr   = JIMI_PVOID_ADD(endAddr, alignment * extraTail);

    // Get the total of display lines
    lineTotal = ((size_t)endAddr - (size_t)startAddr + (alignment - 1)) / alignment;

    printf("------------------------------------------------------------------\n");
    printf("  Addr = 0x%08X, Size = %u bytes\n", (unsigned int)p, (unsigned int)size);
    printf("------------------------------------------------------------------\n");
    printf("\n");

    if (p == NULL || p == (void *)0xCCCCCCCC) {
        printf("  Can not read the data from the address.\n");
        printf("\n");
        return;
    }

    printf("  Address    0  1  2  3 |  4  5  6  7 |  8  9  A  B |  C  D  E  F\n");
    printf("------------------------------------------------------------------\n");
    printf("\n");

    addr = (size_t)startAddr;
    for (i = 0; i < lineTotal; ++i) {
        // display format preview
        //printf("  %08X  00 01 02 03   04 05 06 07   08 09 0A 0B   0C 0D 0E 0F\n", (unsigned int)addr);
        printf("  %08X  ", (unsigned int)addr);
        cur = (unsigned char *)addr;
        for (j = 0; j < alignment; ++j) {
            printf("%02X", (unsigned int)(*cur));
            if (j < alignment - 1) {
                if ((j & 3) != 3)
                    printf(" ");
                else
                    printf("   ");
            }
            cur++;
        }
        printf("    ");
        cur = (unsigned char *)addr;
        for (j = 0; j < alignment; ++j) {
            if (*cur >= 128)
                printf("?");
            else if (*cur >= 32)
                printf("%c", (unsigned char)(*cur));
            else
                printf(".");
            cur++;
        }
        printf("\n");
        addr += alignment;
    }

    printf("\n");
}

NS_JIMI_END

/* 因为互相引用的关系, 这两个#include必须放在class DumpUtils()定义的后面. */
#include "jimi/util/DebugUtils.h"
#include "jimi/util/ReleaseUtils.h"

#endif  /* !_JIMI_UTIL_DUMPUTILS_H_ */
