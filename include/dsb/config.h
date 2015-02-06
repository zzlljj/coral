/*
This header file is meant for internal use in this library, and should
normally not be included directly by client code.  Its purpose is to
aid in maintaining a cross-platform code base.

Note to contributors:
This file intentionally has a ".h" extension, as it is supposed to
be a valid C header.  C++-specific code should therefore be placed in
#ifdef __cplusplus blocks.
*/
#ifndef DSB_CONFIG_H
#define DSB_CONFIG_H

// The 'final' keyword (C++11) was introduced in Visual Studio 2012.
// Before that (since VS2005), 'sealed' was used for the same purpose.
#ifdef __cplusplus
#   if defined(_MSC_VER) && _MSC_VER < 1700
#       define DSB_FINAL sealed
#   else
#       define DSB_FINAL final
#   endif
#endif

// The 'noexcept' keyword (C++11) is not yet supported by Visual Studio.
#ifdef __cplusplus
#   ifdef _MSC_VER
#       define DSB_NOEXCEPT
#   else
#       define DSB_NOEXCEPT noexcept
#   endif
#endif

// Visual Studio versions prior to 2012 have a different emplace_back()
// signature than the standard one, due to lack of support for variadic
// templates.
#ifdef __cplusplus
#   if defined(_MSC_VER) && _MSC_VER < 1700
#       define DSB_USE_MSVC_EMPLACE_WORKAROUND 1
#   else
#       define DSB_USE_MSVC_EMPLACE_WORKAROUND 0
#   endif
#endif

#endif // header guard
