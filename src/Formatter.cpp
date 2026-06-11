#include "yaodaq/Formatter.hpp"

#include <simdjson.h>

static std::string format_json( const simdjson::dom::element& e, size_t indent );
std::string        yaodaq::Formatter::try_parse_json( std::string_view sv, size_t indent )
{
  static thread_local simdjson::dom::parser parser;

  simdjson::padded_string padded( sv );
  simdjson::dom::element  e;

  if( parser.parse( padded ).get( e ) != simdjson::SUCCESS ) return {};

  return format_json( e, indent );
}

// SIMDJSON IMPLEMENTATION
static std::string format_json( const simdjson::dom::element& e, size_t indent )
{
  switch( e.type() )
  {
    case simdjson::dom::element_type::OBJECT:
    {
      simdjson::dom::object obj = e;
      if( obj.size() == 0 ) return yaodaq::punct( "{}" );

      std::string out = yaodaq::punct( "{" ) + '\n';
      for( auto [k, v]: obj )
      {
        out += std::string( indent * 2 + 2, ' ' );
        out += yaodaq::key_color( fmt::format( "\"{}\"", std::string( k ) ) );
        out += yaodaq::punct( ": " );
        out += format_json( v, indent + 1 );  // RECURSION HERE
        out += "\n";
      }
      out += std::string( indent * 2, ' ' ) + yaodaq::punct( "}" );
      return out;
    }

    case simdjson::dom::element_type::ARRAY:
    {
      simdjson::dom::array arr = e;
      if( arr.size() == 0 ) return yaodaq::punct( "[]" );

      std::string out = yaodaq::punct( "[" ) + '\n';
      size_t      i   = 0;
      for( auto v: arr )
      {
        out += std::string( indent * 2 + 2, ' ' );
        out += format_json( v, indent + 1 );  // RECURSION HERE
        if( ++i != arr.size() ) out += yaodaq::punct( "," );
        out += "\n";
      }
      out += std::string( indent * 2, ' ' ) + yaodaq::punct( "]" );
      return out;
    }

    case simdjson::dom::element_type::STRING:
    {
      // Avoid extra std::string copy by passing string_view to colorizer
      auto s = e.get_string().value_unsafe();
      return yaodaq::string_color( s );
    }
    case simdjson::dom::element_type::INT64: return yaodaq::number_int_color( e.get_int64() );
    case simdjson::dom::element_type::UINT64: return yaodaq::number_int_color( static_cast<long long>( e.get_uint64() ) );
    case simdjson::dom::element_type::DOUBLE: return yaodaq::number_float_color( e.get_double() );
    case simdjson::dom::element_type::BOOL: return yaodaq::bool_color( e.get_bool() );
    case simdjson::dom::element_type::NULL_VALUE: return yaodaq::null_color();
    default: return yaodaq::string_color( "[unsupported]" );
  }
}
