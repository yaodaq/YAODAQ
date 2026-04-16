#include "yaodaq/Response.hpp"

#include "yaodaq/Formatter.hpp"

#include <cpp-terminal/screen.hpp>

std::string yaodaq::Response::pretty_format()
{
  if( !m_json.is_array() ) return fmt::format( fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold, "No result array found\n" );
  return Formatter::format( m_json );
}
