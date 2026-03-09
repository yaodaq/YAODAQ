#include "yaodaq/JsonRPCHandler.hpp"
#if defined( _WIN32 )
  #include <windows.h>
#endif
#include <chrono>
#include <cstdint>

std::int64_t yaodaq::JsonRPCHandler::generateID()
{
#if defined( _WIN32 )
  // Use system_clock for epoch reference
  static auto base_time = std::chrono::system_clock::now();

  // Use high-resolution counter for precise timing
  static LARGE_INTEGER freq;
  static bool          init = false;
  static LARGE_INTEGER base_counter;

  if( !init )
  {
    QueryPerformanceFrequency( &freq );
    QueryPerformanceCounter( &base_counter );
    init = true;
  }

  LARGE_INTEGER now_counter;
  QueryPerformanceCounter( &now_counter );

  // Calculate delta in nanoseconds
  int64_t delta_ns = ( now_counter.QuadPart - base_counter.QuadPart ) * 1'000'000'000 / freq.QuadPart;

  // Base time in nanoseconds
  auto base_ns = std::chrono::duration_cast<std::chrono::nanoseconds>( base_time.time_since_epoch() ).count();

  return base_ns + delta_ns;

#else
  return std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
#endif
}
