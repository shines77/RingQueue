
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

// Class CValueEvent() use in C++ only
#ifdef __cplusplus

template <typename T>
class CValueEvent
{
private:
    T value;

public:
    CValueEvent() : value(0) {}

    CValueEvent(const T & value_) : value(value_) {}

#if 0
    // Copy constructor
    CValueEvent(const CValueEvent & src) : value(src.value) {
        //
    }
#endif
    // Copy constructor
    CValueEvent(const volatile CValueEvent & src) : value(src.value) {
        //
    }

#if 0
  #if 0
    // Copy assignment operator
    CValueEvent & operator = (const CValueEvent & rhs) {
        this->value = rhs.value;
        return *this;
    }
  #endif

    // Copy assignment operator
    CValueEvent & operator = (const volatile CValueEvent & rhs) {
        this->value = rhs.value;
        return *this;
    }
#else
  #if 0
    // Copy assignment operator
    void operator = (const CValueEvent & rhs) {
        this->value = rhs.value;
    }
  #endif

    // Copy assignment operator
    void operator = (const volatile CValueEvent & rhs) {
        this->value = rhs.value;
    }
#endif

    T getValue() const {
        return value;
    }

    void setValue(T newValue) {
        value = newValue;
    }

    // Read data from event
    void read(CValueEvent & event) const {
        event.value = this->value;
    }

    // Copy data from src
    void copy(const CValueEvent & src) {
        value = src.value;
    }

    // Update data from event
    void update(const CValueEvent & event) {
        this->value = event.value;
    }

    // Move the data reference only
    void move(CValueEvent & event) {
        // Do nothing!
    }

    ////////////////////////////////////////////////////////////////////////////
    // volatile operation
    ////////////////////////////////////////////////////////////////////////////

    // Read data from event
    void read(volatile CValueEvent & event) {
        event.value = this->value;
    }

    // Copy data from src
    void copy(const volatile CValueEvent & src) {
        value = src.value;
    }

    // Update data from event
    void update(const volatile CValueEvent & event) {
        this->value = event.value;
    }

    // Move the data reference only
    void move(volatile CValueEvent & event) {
        // Do nothing!
    }
};

#endif  /* __cplusplus */

#endif  /* _JIMI_MESSAGE_EVENT_H_ */
