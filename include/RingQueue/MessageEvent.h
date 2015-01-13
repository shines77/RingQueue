
#ifndef _JIMI_MESSAGE_EVENT_H_
#define _JIMI_MESSAGE_EVENT_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "vs_stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MessageEvent
{
    uint64_t value;
};

typedef struct MessageEvent MessageEvent;

#ifdef __cplusplus
}
#endif

#endif  /* _JIMI_MESSAGE_EVENT_H_ */
