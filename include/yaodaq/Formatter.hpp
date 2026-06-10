#pragma once
#include "yaodaq/Export.hpp"

#include <fmt/color.h>
#include <fmt/core.h>
#include <map>
#include <memory>
#include <optional>
#include <simdjson.h>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace yaodaq
{

// ============================================================
// COLOR HELPERS
// ============================================================

inline std::string key_color( const std::string_view s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::cornflower_blue ) | fmt::emphasis::bold ) ); }

inline std::string string_color( const std::string_view s ) { return fmt::format( "{}", fmt::styled( "\"" + std::string( s ) + "\"", fmt::fg( fmt::color::light_green ) ) ); }

inline std::string number_int_color( const long long v ) { return fmt::format( "{}", fmt::styled( v, fmt::fg( fmt::color::plum ) ) ); }

inline std::string number_float_color( const double v ) { return fmt::format( "{}", fmt::styled( v, fmt::fg( fmt::color::medium_purple ) ) ); }

inline std::string bool_color( const bool v )
{
  if( v ) return fmt::format( "{}", fmt::styled( "true", fmt::fg( fmt::color::green ) | fmt::emphasis::bold ) );
  else
    return fmt::format( "{}", fmt::styled( "false", fmt::fg( fmt::color::red ) | fmt::emphasis::bold ) );
}

inline std::string bool_color( const std::nullptr_t ) { return fmt::format( "{}", fmt::styled( "nullptr", fmt::fg( fmt::color::red ) | fmt::emphasis::bold ) ); }

inline std::string null_color() { return fmt::format( "{}", fmt::styled( "null", fmt::fg( fmt::color::gray ) ) ); }

inline std::string punct( const std::string& s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::gray ) ) ); }

// ============================================================
// FORMATTER
// ============================================================

class Formatter
{
public:
  template<typename T> YAODAQ_API static std::string format( const T& v, const std::size_t indent = 0 ) { return format_impl( v, indent ); }

private:
  // ============================================================
  // STRINGS (SMART JSON DETECTION)
  // ============================================================

  YAODAQ_API static std::string format_impl( const std::string& v, const std::size_t indent );
  YAODAQ_API static std::string format_impl( const std::string_view v, const std::size_t indent );
  YAODAQ_API static std::string format_impl( const char* const v, const std::size_t indent );
  YAODAQ_API static std::string format_impl( const simdjson::dom::element& e, const std::size_t indent );

  // ============================================================
  // BOOL
  // ============================================================

  YAODAQ_API static std::string format_impl( const bool v, const std::size_t ) { return bool_color( v ); }

  // ============================================================
  // ARITHMETIC
  // ============================================================

  template<typename T> static std::enable_if_t<std::is_arithmetic_v<T>, std::string> format_impl( const T v, const std::size_t )
  {
    if constexpr( std::is_floating_point_v<T> ) return number_float_color( (double)v );
    else
      return number_int_color( (long long)v );
  }

  // ============================================================
  // OPTIONAL
  // ============================================================

  template<typename T> static std::string format_impl( const std::optional<T>& v, const std::size_t indent )
  {
    if( !v ) return null_color();
    return format( *v, indent );
  }

  // ============================================================
  // SMART POINTERS
  // ============================================================

  template<typename T> static std::string format_impl( const std::shared_ptr<T>& v, const std::size_t indent )
  {
    if( !v ) return null_color();
    return format( *v, indent );
  }

  template<typename T> static std::string format_impl( const std::unique_ptr<T>& v, const std::size_t indent )
  {
    if( !v ) return null_color();
    return format( *v, indent );
  }

  // ============================================================
  // SPAN
  // ============================================================

  template<typename T> static std::string format_impl( const std::span<const T> s, const std::size_t indent )
  {
    if( s.empty() ) return punct( "[]" );
    std::string out = punct( "[" ) + "\n";
    for( std::size_t i = 0; i < s.size(); ++i )
    {
      out += std::string( indent * 2 + 2, ' ' );
      out += format( s[i], indent + 1 );
      if( i + 1 != s.size() ) out += punct( "," );
      out += "\n";
    }
    out += std::string( indent * 2, ' ' ) + punct( "]" );
    return out;
  }

  // vector → span
  template<typename T> static std::string format_impl( const std::vector<T>& v, const std::size_t indent ) { return format_impl( std::span<const T>( v ), indent ); }

  // ============================================================
  // MAPS
  // ============================================================

  template<typename K, typename V> static std::string format_impl( const std::map<K, V>& m, const std::size_t indent )
  {
    if( m.empty() ) return punct( "{}" );
    std::string out = punct( "{" ) + "\n";
    for( auto it = m.begin(); it != m.end(); ++it )
    {
      out += std::string( indent * 2 + 2, ' ' );
      out += key_color( fmt::format( "\"{}\"", it->first ) );
      out += punct( ": " );
      out += format( it->second, indent + 1 );
      if( std::next( it ) != m.end() ) out += punct( "," );
      out += "\n";
    }
    out += std::string( indent * 2, ' ' ) + punct( "}" );
    return out;
  }

  template<typename K, typename V> static std::string format_impl( const std::unordered_map<K, V>& m, const std::size_t indent )
  {
    if( m.empty() ) return punct( "{}" );
    std::string out = punct( "{" ) + "\n";
    for( auto it = m.begin(); it != m.end(); ++it )
    {
      out += std::string( indent * 2 + 2, ' ' );
      out += key_color( fmt::format( "\"{}\"", it->first ) );
      out += punct( ": " );
      out += format( it->second, indent + 1 );
      if( std::next( it ) != m.end() ) out += punct( "," );
      out += "\n";
    }
    out += std::string( indent * 2, ' ' ) + punct( "}" );
    return out;
  }

  // ============================================================
  // FALLBACK (SAFE)
  // ============================================================
  template<typename T> static std::string format_impl( const T&, const std::size_t ) { return string_color( fmt::format( "[unformattable type: {}]", typeid( T ).name() ) ); }
};

}  // namespace yaodaq
