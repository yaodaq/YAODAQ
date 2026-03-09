#include "yaodaq/Identifier.hpp"

#include "magic_enum/magic_enum.hpp"
#include "nlohmann/json.hpp"

yaodaq::Identifier::operator nlohmann::json() const noexcept
{
  nlohmann::json ret;
  ret["component"] = m_component.str();
  ret["type"]      = m_type;
  ret["name"]      = m_name;
  return ret;
}

yaodaq::Identifier yaodaq::Identifier::createFromstring( const std::string& str )
{
  std::size_t slashCount = std::count( str.begin(), str.end(), '/' );
  if( slashCount != 2 ) throw Exception( "Identifier must contain exactly 2 '/' characters" );

  std::size_t firstSlash  = str.find( '/' );
  std::size_t secondSlash = str.find( '/', firstSlash + 1 );

  auto maybeComponent = magic_enum::enum_cast<Component::Name>( str.substr( 0, firstSlash ) );
  if( !maybeComponent.has_value() ) throw Exception( "Unknown component" );

  Component::Name component = maybeComponent.value();
  // Extract type and name
  std::string     type      = str.substr( firstSlash + 1, secondSlash - firstSlash - 1 );
  std::string     name      = str.substr( secondSlash + 1 );

  return Identifier( component, type, name );
}
