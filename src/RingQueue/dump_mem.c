
#include "dump_mem.h"

#include <stdio.h>
#include <assert.h>

#ifndef jimic_assert
#define jimic_assert    assert
#endif // jimic_assert

/**
 * macro for address aligned to n bytes
 */
#define JIMI_ALIGNED_TO(n, alignment)   \
    (((n) + ((alignment) - 1)) & ~(size_t)((alignment) - 1))

#define JIMI_ADDR_ALIGNED_TO(p, alignment)   \
    ((void *)((((size_t)(void *)(p)) + ((alignment) - 1)) & ~(size_t)((alignment) - 1)))

#define JIMI_ADDR_ALIGNED_DOWNTO(p, alignment)   \
    ((void *)(((size_t)(void *)(p)) & ~(size_t)((alignment) - 1)))

#define JIMI_PVOID_INC(p, n)    ((void *)((size_t)(p) + 1))
#define JIMI_PVOID_DEC(p, n)    ((void *)((size_t)(p) - 1))

#define JIMI_PVOID_ADD(p, n)    ((void *)((size_t)(p) + (n)))
#define JIMI_PVOID_SUB(p, n)    ((void *)((size_t)(p) - (n)))

void memory_dump(void *p, size_t size, bool alignedTo /* = false */,
                 unsigned int alignment /* = 16 */,
                 unsigned int extraHead /* = 0 */,
                 unsigned int extraTail /* = 0 */)
{
    // display memory dump
    size_t i, j;
    size_t addr;
    size_t lineTotal;
    unsigned char *cur;
    void *startAddr, *endAddr;

    jimic_assert(p != NULL);
    //jimic_assert(p != (void *)0xCCCCCCCC);

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
    printf("  Addr = 0x%016p, Size = %u bytes\n", p, (unsigned int)size);
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
