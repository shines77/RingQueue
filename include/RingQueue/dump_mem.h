
#ifndef _RINGQUEUE_DUMP_MEM_H_
#define _RINGQUEUE_DUMP_MEM_H_

#include "stdint.h"
#include <stddef.h>
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

void dump_mem(void *p, size_t size, bool alignedTo /* = false */,
              unsigned int alignment /* = 16 */,
              unsigned int extraHead /* = 0 */,
              unsigned int extraTail /* = 0 */);

#ifdef __cplusplus
}
#endif

#endif  /* _RINGQUEUE_DUMP_MEM_H_ */
