
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

// Class Sequence() use in c++ only
#ifdef __cplusplus

template <typename T>
class CValueEvent
{
public:
    T   value;

#if 0
    CValueEvent & operator = (CValueEvent & rhs) {
        this->value = rhs.value;
        return *this;
    }
#else
   void operator = (CValueEvent & rhs) {
        this->value = rhs.value;
    }
#endif

    void read(CValueEvent & event) {
        event.value = this->value;
    }

    void copy(CValueEvent & src) {
        this->value = src.value;
    }

    void update(CValueEvent & event) {
        this->value = event.value;
    }
};

#endif  /* __cplusplus */

#endif  /* _JIMI_MESSAGE_EVENT_H_ */
