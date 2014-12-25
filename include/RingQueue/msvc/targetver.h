
#ifndef _JIMI_CORE_WIN32_TARGETVER_H_
#define _JIMI_CORE_WIN32_TARGETVER_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#if defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__)

// 以下宏定义要求的最低平台。要求的最低平台
// 是具有运行应用程序所需功能的 Windows、Internet Explorer 等产品的
// 最早版本。通过在指定版本及更低版本的平台上启用所有可用的功能，宏可以
// 正常工作。

// 如果必须要针对低于以下指定版本的平台，请修改下列定义。
// 有关不同平台对应值的最新信息，请参考 MSDN。
#ifndef WINVER                  // 指定要求的最低平台是 Windows XP。
#define WINVER 0x0501           // 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif

/***
 ** NOTE:
 **  _WIN32_WINNT defined as 0x0502(Windows 2003 server) for InterlockedExchangeAdd64()
 **  Same for other functions like InterlockedXXXXX64().
 **/

#ifndef _WIN32_WINNT            // 指定要求的最低平台是 Windows XP。
#define _WIN32_WINNT 0x0502     // 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif

#ifndef _WIN32_WINDOWS          // 指定要求的最低平台是 Windows 98。
#define _WIN32_WINDOWS 0x0410   // 将此值更改为适当的值，以适用于 Windows Me 或更高版本。
#endif

#ifndef _WIN32_IE               // 指定要求的最低平台是 Internet Explorer 6.0。
#define _WIN32_IE 0x0600        // 将此值更改为相应的值，以适用于 IE 的其他版本。
#endif

#endif  /* _WIN32 || __MINGW32__ || __CYGWIN__ */

#endif  /* _JIMI_CORE_WIN32_TARGETVER_H_ */
