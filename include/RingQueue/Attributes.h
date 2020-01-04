
/*
 * See: https://searchfox.org/mozilla-central/source/mfbt/Attributes.h
 *
 */

#ifndef JIMI_ATTRIBUTES_H
#define JIMI_ATTRIBUTES_H

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/*
 * JIMI_ALWAYS_INLINE is a macro which expands to tell the compiler that the
 * method decorated with it must be inlined, even if the compiler thinks
 * otherwise.  This is only a (much) stronger version of the inline hint:
 * compilers are not guaranteed to respect it (although they're much more likely
 * to do so).
 *
 * The JIMI_ALWAYS_INLINE_EVEN_DEBUG macro is yet stronger. It tells the
 * compiler to inline even in DEBUG builds. It should be used very rarely.
 */
#if defined(_MSC_VER)
#  define JIMI_ALWAYS_INLINE_EVEN_DEBUG     __forceinline
#elif defined(__GNUC__)
#  define JIMI_ALWAYS_INLINE_EVEN_DEBUG     __attribute__((always_inline)) inline
#else
#  define JIMI_ALWAYS_INLINE_EVEN_DEBUG     inline
#endif

#if !defined(DEBUG)
#  define JIMI_ALWAYS_INLINE    JIMI_ALWAYS_INLINE_EVEN_DEBUG
#elif defined(_MSC_VER) && !defined(__cplusplus)
#  define JIMI_ALWAYS_INLINE    __inline
#else
#  define JIMI_ALWAYS_INLINE    inline
#endif

#if defined(_MSC_VER)
/*
 * g++ requires -std=c++0x or -std=gnu++0x to support C++11 functionality
 * without warnings (functionality used by the macros below).  These modes are
 * detectable by checking whether __GXX_EXPERIMENTAL_CXX0X__ is defined or, more
 * standardly, by checking whether __cplusplus has a C++11 or greater value.
 * Current versions of g++ do not correctly set __cplusplus, so we check both
 * for forward compatibility.
 */
#  define JIMI_HAVE_NEVER_INLINE    __declspec(noinline)
#  define JIMI_HAVE_NORETURN        __declspec(noreturn)
#elif defined(__clang__)
/*
 * Per Clang documentation, "Note that marketing version numbers should not
 * be used to check for language features, as different vendors use different
 * numbering schemes. Instead, use the feature checking macros."
 */
#  ifndef __has_extension
#    define __has_extension \
      __has_feature /* compatibility, for older versions of clang */
#  endif
#  if __has_attribute(noinline)
#    define JIMI_HAVE_NEVER_INLINE  __attribute__((noinline))
#  endif
#  if __has_attribute(noreturn)
#    define JIMI_HAVE_NORETURN      __attribute__((noreturn))
#  endif
#elif defined(__GNUC__)
#  define JIMI_HAVE_NEVER_INLINE    __attribute__((noinline))
#  define JIMI_HAVE_NORETURN        __attribute__((noreturn))
#  define JIMI_HAVE_NORETURN_PTR    __attribute__((noreturn))
#endif

/*
 * When built with clang analyzer (a.k.a scan-build), define JIMI_HAVE_NORETURN
 * to mark some false positives
 */
#ifdef __clang_analyzer__
#  if __has_extension(attribute_analyzer_noreturn)
#    define JIMI_HAVE_ANALYZER_NORETURN     __attribute__((analyzer_noreturn))
#  endif
#endif

/*
 * JIMI_NEVER_INLINE is a macro which expands to tell the compiler that the
 * method decorated with it must never be inlined, even if the compiler would
 * otherwise choose to inline the method.  Compilers aren't absolutely
 * guaranteed to support this, but most do.
 */
#if defined(JIMI_HAVE_NEVER_INLINE)
#  define JIMI_NEVER_INLINE     JIMI_HAVE_NEVER_INLINE
#else
#  define JIMI_NEVER_INLINE     /* no support */
#endif

/*
 * JIMI_NEVER_INLINE_DEBUG is a macro which expands to JIMI_NEVER_INLINE
 * in debug builds, and nothing in opt builds.
 */
#if defined(DEBUG)
#  define JIMI_NEVER_INLINE_DEBUG   JIMI_NEVER_INLINE
#else
#  define JIMI_NEVER_INLINE_DEBUG   /* don't inline in opt builds */
#endif
/*
 * JIMI_NORETURN, specified at the start of a function declaration, indicates
 * that the given function does not return.  (The function definition does not
 * need to be annotated.)
 *
 *   JIMI_NORETURN void abort(const char* msg);
 *
 * This modifier permits the compiler to optimize code assuming a call to such a
 * function will never return.  It also enables the compiler to avoid spurious
 * warnings about not initializing variables, or about any other seemingly-dodgy
 * operations performed after the function returns.
 *
 * There are two variants. The GCC version of NORETURN may be applied to a
 * function pointer, while for MSVC it may not.
 *
 * This modifier does not affect the corresponding function's linking behavior.
 */
#if defined(JIMI_HAVE_NORETURN)
#  define JIMI_NORETURN         JIMI_HAVE_NORETURN
#else
#  define JIMI_NORETURN         /* no support */
#endif
#if defined(JIMI_HAVE_NORETURN_PTR)
#  define JIMI_NORETURN_PTR     JIMI_HAVE_NORETURN_PTR
#else
#  define JIMI_NORETURN_PTR     /* no support */
#endif

#endif // JIMI_ATTRIBUTES_H
