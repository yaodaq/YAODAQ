#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/Parameters.hpp"

#include <fmt/color.h>
#include <fmt/core.h>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace yaodaq
{

// ============================================================
// COLOR HELPERS
// ============================================================

inline std::string key_color( std::string_view s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::cornflower_blue ) | fmt::emphasis::bold ) ); }

inline std::string string_color( std::string_view s ) { return fmt::format( "{}", fmt::styled( "\"" + std::string( s ) + "\"", fmt::fg( fmt::color::light_green ) ) ); }

inline std::string number_int_color( long long v ) { return fmt::format( "{}", fmt::styled( v, fmt::fg( fmt::color::plum ) ) ); }

inline std::string number_float_color( double v ) { return fmt::format( "{}", fmt::styled( v, fmt::fg( fmt::color::medium_purple ) ) ); }

inline std::string bool_color( bool v ) { return fmt::format( "{}", fmt::styled( v ? "true" : "false", fmt::fg( v ? fmt::color::green : fmt::color::red ) | fmt::emphasis::bold ) ); }

inline std::string null_color() { return fmt::format( "{}", fmt::styled( "null", fmt::fg( fmt::color::gray ) ) ); }

inline std::string punct( std::string_view s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::gray ) ) ); }

class Formatter
{
public:
  template<typename T> static std::string format( const T& v, const std::size_t indent = 0 ) { return format_value( v, indent ); }

private:
  // BASIC TYPES
  YAODAQ_API static std::string format_value( const bool v, const std::size_t ) { return bool_color( v ); }

  template<typename T> static std::enable_if_t<std::is_arithmetic_v<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, bool>, std::string> format_value( const T v, const std::size_t )
  {
    if constexpr( std::is_floating_point_v<std::decay_t<T>> ) return number_float_color( static_cast<double>( v ) );
    else
      return number_int_color( static_cast<long long>( v ) );
  }
  // STRING JSON ENTRY
  YAODAQ_API static std::string try_parse_json( const std::string_view sv, const std::size_t indent );
  YAODAQ_API static std::string format_value( const std::string_view v, const std::size_t indent )
  {
    if( auto json = try_parse_json( v, indent ); !json.empty() ) return json;
    return string_color( v );
  }
  YAODAQ_API static std::string           format_value( const std::string& v, const std::size_t indent ) { return format_value( std::string_view( v ), indent ); }
  YAODAQ_API static std::string           format_value( const char* const v, const std::size_t indent ) { return format_value( std::string_view( v ? v : "" ), indent ); }
  // CONTAINERS
  template<typename T> static std::string format_value( const std::span<const T> s, const std::size_t indent )
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
  template<typename K, typename V> static std::string format_value( const std::map<K, V>& m, const std::size_t indent )
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
  template<typename K, typename V> static std::string format_value( const std::unordered_map<K, V>& m, size_t indent )
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
  // POINTERS / OPTIONAL
  template<typename T> static std::string format_value( const std::optional<T>& v, const std::size_t indent )
  {
    if( !v ) return null_color();
    return format( *v, indent );
  }
  template<typename T> static std::string format_value( const std::shared_ptr<T>& v, const std::size_t indent ) { return v ? format( *v, indent ) : null_color(); }
  template<typename T> static std::string format_value( const std::unique_ptr<T>& v, const std::size_t indent ) { return v ? format( *v, indent ) : null_color(); }
  // PARAMETERS
  YAODAQ_API static std::string           format_value( const Parameters& p, size_t indent )
  {
    if( p.size() == 0 ) return punct( "{}" );
    std::string out = punct( "{" ) + "\n";

    p.visit_all(
      [&]( const std::string& key, const Parameters::parameter& value )
      {
        out += std::string( indent * 2 + 2, ' ' );
        out += key_color( fmt::format( "\"{}\"", key ) );
        out += punct( ": " );

        out += std::visit(
          [&]( const auto& v ) -> std::string
          {
            using T = std::decay_t<decltype( v )>;
            if constexpr( std::is_same_v<T, bool> ) return bool_color( v );
            else if constexpr( std::is_integral_v<T> )
              return number_int_color( (long long)v );
            else if constexpr( std::is_floating_point_v<T> )
              return number_float_color( v );
            else if constexpr( std::is_same_v<T, std::string> )
              return Formatter::format( std::string_view( v ), indent + 1 );
            else if constexpr( std::is_same_v<T, Parameters::int_list> || std::is_same_v<T, Parameters::double_list> || std::is_same_v<T, Parameters::string_list> )
              return Formatter::format( std::span( v ), indent + 1 );
            else
              return string_color( "[unformattable]" );
          },
          value );
        out += "\n";
      } );

    out += std::string( indent * 2, ' ' ) + punct( "}" );
    return out;
  }
  // FALLBACK
  template<typename T> static std::enable_if_t<!std::is_arithmetic_v<T> && !std::is_same_v<std::decay_t<T>, std::string>, std::string> format_value( const T&, const std::size_t ) { return string_color( "[unformattable]" ); }
};

}  // namespace yaodaq
