
#ifndef _JIMIC_SYSTEM_SLEEP_H_
#define _JIMIC_SYSTEM_SLEEP_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER) || defined(__INTEL_COMPILER) || defined(__MINGW32__)
//__declspec(dllimport) int __stdcall SwitchToThread(void);
#endif

/* Sleep for the platform */
void jimi_sleep(unsigned int millisec);

/* Sleep for Linux or MinGW */
void jimi_usleep(unsigned int usec);

/* Sleep for Windows or MinGW */
void jimi_wsleep(unsigned int millisec);

/* Yield(): On Windows: Switch to the other threads in the same CPU core. */
/*          On Linux: Switch to the other threads in all cores. */
/* On success, jimi_yield() returns 1.  On error, 0 is returned. */
int jimi_yield();

#ifdef __cplusplus
}
#endif

#endif  /* !_JIMIC_SYSTEM_SLEEP_H_ */
