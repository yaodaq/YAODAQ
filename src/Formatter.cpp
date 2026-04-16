#include "yaodaq/Formatter.hpp"

#include <fmt/color.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

// --- Color helpers ---
inline std::string key_color( const std::string& s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::cornflower_blue ) | fmt::emphasis::bold ) ); }

inline std::string string_color( const std::string& s ) { return fmt::format( "{}", fmt::styled( "\"" + s + "\"", fmt::fg( fmt::color::light_green ) ) ); }

inline std::string number_int_color( long long v ) { return fmt::format( "{}", fmt::styled( v, fmt::fg( fmt::color::plum ) ) ); }

inline std::string number_float_color( double v ) { return fmt::format( "{}", fmt::styled( v, fmt::fg( fmt::color::medium_purple ) ) ); }

inline std::string bool_color( bool v ) { return fmt::format( "{}", fmt::styled( v ? "true" : "false", fmt::fg( fmt::color::orange ) | fmt::emphasis::bold ) ); }

inline std::string null_color() { return fmt::format( "{}", fmt::styled( "null", fmt::fg( fmt::color::gray ) ) ); }

inline std::string punct( const std::string& s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::gray ) ) ); }

std::string yaodaq::Formatter::format( const nlohmann::json& j, const std::size_t indent )
{
  std::string out;
  std::string pad( indent * 2, ' ' );

  if( j.is_object() )
  {
    out += punct( "{" ) + "\n";

    auto it = j.begin();
    while( it != j.end() )
    {
      out += pad + "  ";
      out += key_color( "\"" + it.key() + "\"" );
      out += punct( ": " );

      out += format( it.value(), indent + 1 );

      ++it;
      if( it != j.end() ) out += punct( "," );
      out += "\n";
    }

    out += pad + punct( "}" );
  }
  else if( j.is_array() )
  {
    if( j.empty() ) { out += punct( "[]" ); }
    else
    {
      out += punct( "[" ) + "\n";

      for( size_t i = 0; i < j.size(); ++i )
      {
        out += pad + "  ";
        out += format( j[i], indent + 1 );

        if( i + 1 != j.size() ) out += punct( "," );
        out += "\n";
      }

      out += pad + punct( "]" );
    }
  }
  else if( j.is_string() ) { out += string_color( j.get<std::string>() ); }
  else if( j.is_number_integer() ) { out += number_int_color( j.get<long long>() ); }
  else if( j.is_number_float() ) { out += number_float_color( j.get<double>() ); }
  else if( j.is_boolean() ) { out += bool_color( j.get<bool>() ); }
  else if( j.is_null() ) { out += null_color(); }

  return out;
}
