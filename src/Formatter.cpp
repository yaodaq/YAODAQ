#include "yaodaq/Formatter.hpp"

#include <cstddef>
#include <optional>
#include <simdjson.h>
#include <string>
#include <string_view>

static inline std::optional<simdjson::dom::element> try_parse_json( const std::string_view sv )
{
  static thread_local simdjson::dom::parser parser;
  simdjson::padded_string                   padded( sv );
  simdjson::dom::element                    e;
  if( parser.parse( padded ).get( e ) != simdjson::SUCCESS ) return std::nullopt;
  return e;
}

static std::string format_impl( const simdjson::dom::element& e, const std::size_t indent )
{
  switch( e.type() )
  {
    case simdjson::dom::element_type::OBJECT:
    {
      simdjson::dom::object obj( e );
      if( obj.size() == 0 ) return yaodaq::punct( "{}" );
      std::string out = yaodaq::punct( "{" ) + "\n";
      for( const auto&& [key, value]: obj )
      {
        out += std::string( indent * 2 + 2, ' ' );
        out += yaodaq::key_color( fmt::format( "\"{}\"", std::string( key ) ) );
        out += yaodaq::punct( ": " );
        out += yaodaq::Formatter::format( value, indent + 1 );
        out += "\n";
      }
      out += std::string( indent * 2, ' ' ) + yaodaq::punct( "}" );
      return out;
    }
    case simdjson::dom::element_type::ARRAY:
    {
      simdjson::dom::array arr( e );
      if( arr.size() == 0 ) return yaodaq::punct( "[]" );
      std::string out = yaodaq::punct( "[" ) + "\n";
      std::size_t i{ 0 };
      for( const auto&& v: arr )
      {
        out += std::string( indent * 2 + 2, ' ' );
        out += yaodaq::Formatter::format( v, indent + 1 );
        if( ++i != arr.size() ) out += yaodaq::punct( "," );
        out += '\n';
      }
      out += std::string( indent * 2, ' ' ) + yaodaq::punct( "]" );
      return out;
    }
    case simdjson::dom::element_type::STRING: return yaodaq::string_color( std::string( e.get_string().value_unsafe() ) );
    case simdjson::dom::element_type::INT64: return yaodaq::number_int_color( e.get_int64() );
    case simdjson::dom::element_type::UINT64: return yaodaq::number_int_color( (long long)e.get_uint64() );
    case simdjson::dom::element_type::DOUBLE: return yaodaq::number_float_color( e.get_double() );
    case simdjson::dom::element_type::BOOL: return yaodaq::bool_color( e.get_bool() );
    case simdjson::dom::element_type::NULL_VALUE: return yaodaq::null_color();
    default: return yaodaq::string_color( "[unsupported]" );
  }
}

std::string yaodaq::Formatter::format_impl( const std::string& v, const std::size_t indent )
{
  if( auto j = try_parse_json( v ) ) return format( *j, indent );

  return string_color( v );
}

std::string yaodaq::Formatter::format_impl( const std::string_view v, const std::size_t indent )
{
  if( auto j = try_parse_json( v ) ) return format( *j, indent );

  return string_color( std::string( v ) );
}

std::string yaodaq::Formatter::format_impl( const char* const v, const std::size_t indent )
{
  if( !v ) return null_color();

  std::string_view sv( v );

  if( auto j = try_parse_json( sv ) ) return format( *j, indent );

  return string_color( std::string( v ) );
}
