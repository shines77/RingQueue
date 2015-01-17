
#ifndef _RINGQUEUE_DUMP_MEM_H_
#define _RINGQUEUE_DUMP_MEM_H_

#include <stddef.h>
#include "vs_stdint.h"
#include "vs_stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

void dump_memory(void *p, size_t size, bool alignedTo /* = false */,
                 unsigned int alignment /* = 16 */,
                 unsigned int extraHead /* = 0 */,
                 unsigned int extraTail /* = 0 */);

#ifdef __cplusplus
}
#endif

#endif  /* _RINGQUEUE_DUMP_MEM_H_ */
