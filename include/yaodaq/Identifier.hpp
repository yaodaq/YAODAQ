#pragma once
#include "yaodaq/Component.hpp"
#include "yaodaq/Exception.hpp"
#include "yaodaq/Export.hpp"

#include <string>
#include <string_view>

namespace yaodaq
{

class Identifier
{
public:
  YAODAQ_API Identifier( const Component::Role role, const std::string_view type, const std::string_view name ) :
    m_component( role ), m_type(
                           [type]
                           {
                             is_valid( type, "type" );
                             return type;
                           }() ),
    m_name(
      [name]
      {
        is_valid( name, "name" );
        return name;
      }() )
  {
  }
  YAODAQ_API yaodaq::Component component() const noexcept { return m_component; }
  YAODAQ_API std::string_view type() const noexcept { return m_type; }
  YAODAQ_API std::string_view name() const noexcept { return m_name; }
  YAODAQ_API std::string id() const { return std::string( m_component ) + '/' + m_type + '/' + m_name; }
  YAODAQ_API bool        operator<( const Identifier& id ) const { return this->id() < id.id(); };
  YAODAQ_API bool        operator==( const Identifier& id ) const noexcept
  {
    if( m_component != id.component() ) return false;
    if( m_type != id.type() ) return false;
    if( m_name != id.name() ) return false;
    else
      return true;
  }
  YAODAQ_API static Identifier createFromstring( const std::string_view str );

private:
  YAODAQ_API explicit Identifier() = default;
  yaodaq::Component m_component;
  std::string       m_type{ "YAODAQ" };
  std::string       m_name{ "Unnamed" };
  static void       is_valid( const std::string_view s, std::string_view which )
  {
    if( s.find( '/' ) != std::string::npos ) throw Exception( std::string( which ) + ": " + std::string( s ) + " contains the forbidden character '/'" );
  }
};

}  // namespace yaodaq

template<> struct std::hash<yaodaq::Identifier>
{
  std::size_t operator()( yaodaq::Identifier const& s ) const noexcept { return std::hash<std::string>{}( s.id() ); }
};
