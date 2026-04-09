#include <algorithm>
#include <cstddef>
#include <magic_enum/magic_enum.hpp>
#include <string_view>
#include <yaodaq/Exception.hpp>
#include <yaodaq/Identifier.hpp>

yaodaq::Identifier yaodaq::Identifier::createFromstring( const std::string_view str )
{
  const std::size_t slashCount{ static_cast<std::size_t>( std::count( str.begin(), str.end(), '/' ) ) };
  if( slashCount != 2 ) throw Exception( "Identifier must contain exactly 2 '/' characters" );

  const std::size_t firstSlash{ str.find( '/' ) };
  const std::size_t secondSlash{ str.find( '/', firstSlash + 1 ) };

  auto maybeComponent = magic_enum::enum_cast<Component::Role>( str.substr( 0, firstSlash ) );
  if( !maybeComponent.has_value() ) throw Exception( "Unknown component" );

  const Component::Role component{ maybeComponent.value() };

  const std::string type{ str.substr( firstSlash + 1, secondSlash - firstSlash - 1 ) };
  const std::string name{ str.substr( secondSlash + 1 ) };

  return Identifier( component, type, name );
}
