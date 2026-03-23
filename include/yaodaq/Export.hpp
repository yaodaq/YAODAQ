#pragma once

#if defined( _WIN32 ) || defined( _WIN64 )
  // On Windows, use __declspec(dllexport/dllimport)
  #ifdef YAODAQ_BUILD_DLL
    #define YAODAQ_API __declspec( dllexport )
  #else
    #define YAODAQ_API __declspec( dllimport )
  #endif
#else
  #if defined( __GNUC__ ) || defined( __clang__ )
    #define YAODAQ_API __attribute__( ( visibility( "default" ) ) )
  #else
    #define YAODAQ_API
  #endif
#endif

#if defined( __GNUC__ ) || defined( __clang__ )
  #define YAODAQ_INTERNAL __attribute__( ( visibility( "hidden" ) ) )
#else
  #define YAODAQ_INTERNAL
#endif
