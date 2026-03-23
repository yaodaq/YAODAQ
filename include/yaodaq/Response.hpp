#pragma once

#include "yaodaq/Identifier.hpp"

#include <cpp-terminal/screen.hpp>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <new>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <yaodaq/Export.hpp>
namespace yaodaq
{

// A response to a JsonRPC
class Response
{
public:
  YAODAQ_API explicit Response( const nlohmann::json& response ) : m_json( response ) {}
  YAODAQ_API std::string_view dump( const std::size_t j ) { return m_json.dump( j ); }
  YAODAQ_API                  operator const nlohmann::json&() const noexcept { return m_json; }
  YAODAQ_API                  operator nlohmann::json() noexcept { return m_json; }
  // Pretty formatter for JSON-RPC array of results/errors
  YAODAQ_API std::string pretty_format();

private:
  std::string color_json( const nlohmann::json& j, int indent = 0 )
  {
    std::string s;
    std::string pad( indent * 2, ' ' );

    if( j.is_object() )
    {
      s += pad + "{\n";
      for( auto it = j.begin(); it != j.end(); ++it )
      {
        s += pad + fmt::format( "{}:\n", fmt::styled( it.key(), fmt::fg( fmt::color::blue ) | fmt::emphasis::bold ) );

        // Recursive call
        s += color_json( it.value(), indent + 1 );

        // Add comma if not last
        if( std::next( it ) != j.end() ) s += ",\n";
        else
          s += "\n";
      }
      s += pad + "}";
    }
    else if( j.is_array() )
    {
      s += pad + "[\n";
      for( size_t i = 0; i < j.size(); ++i )
      {
        s += pad + "  " + color_json( j[i], indent + 1 );

        // Add comma if not last
        if( i + 1 != j.size() ) s += ",\n";
        else
          s += "\n";
      }
      s += pad + "]";
    }
    else if( j.is_string() ) { s += fmt::format( "{}", fmt::styled( "\"" + j.get<std::string>() + "\"", fmt::fg( fmt::color::green ) ) ); }
    else if( j.is_number_integer() ) { s += fmt::format( "{}", fmt::styled( j.get<int>(), fmt::fg( fmt::color::magenta ) ) ); }
    else if( j.is_number_float() ) { s += fmt::format( "{}", fmt::styled( j.get<double>(), fmt::fg( fmt::color::magenta ) ) ); }
    else if( j.is_boolean() ) { s += fmt::format( "{}", fmt::styled( j.get<bool>() ? "true" : "false", fmt::fg( fmt::color::yellow ) | fmt::emphasis::bold ) ); }
    else if( j.is_null() ) { s += fmt::format( "{}", fmt::styled( "null", fmt::fg( fmt::color::gray ) ) ); }

    return s;
  }

  nlohmann::json m_json;
};

}  // namespace yaodaq
