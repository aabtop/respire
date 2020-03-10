#ifndef _STDEXT_SHARED_LIBRARY_H_
#define _STDEXT_SHARED_LIBRARY_H_

#if defined(_MSC_VER)
  #if defined(SHARED_LIBRARY_EXPORT)
      #define PUBLIC_API __declspec(dllexport)
  #else
      #define PUBLIC_API __declspec(dllimport)
  #endif
#else
  #if defined(SHARED_LIBRARY_EXPORT)
      #define PUBLIC_API __attribute__ ((visibility ("default")))
  #else
      #define PUBLIC_API
  #endif
#endif

#endif  // _STDEXT_SHARED_LIBRARY_H_
