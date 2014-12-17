
#ifndef _JIMIC_SYSTEM_CONSOLE_H_
#define _JIMIC_SYSTEM_CONSOLE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void jimi_cpu_warmup(int delayTime);

int jimi_console_readkey(bool enabledCpuWarmup, bool displayTips,
                         bool echoInput);

int jimi_console_readkey_newline(bool enabledCpuWarmup, bool displayTips,
                                 bool echoInput);

#ifdef __cplusplus
}
#endif

#endif  /* !_JIMIC_SYSTEM_CONSOLE_H_ */
