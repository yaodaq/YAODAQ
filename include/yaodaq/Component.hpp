#pragma once
#include "yaodaq/Export.hpp"

#include <cstdint>
#include <string>

namespace yaodaq
{

class Component
{
public:
  enum class Role : std::uint_least8_t
  {
    Server = 1,  //< A Yaodaq server
    Client,
    Module,  //< A specific module within the Yaodaq system
    Board,   //< A board, presumably a hardware or logical component
    Controller,
    Logger,
    Browser = 100,  //< A client that is a browser (not a Yaodaq client)
  };
  YAODAQ_API Component() noexcept = delete;
  YAODAQ_API explicit Component( const Component::Role role ) noexcept : m_role( role ) {}
  YAODAQ_API explicit operator std::string() const;
  YAODAQ_API explicit operator int() const noexcept { return static_cast<int>( m_role ); }
  YAODAQ_API Role     role() const noexcept { return static_cast<Role>( m_role ); }
  YAODAQ_API bool     operator==( const Component& comp ) const noexcept
  {
    if( m_role == comp.m_role ) return true;
    else
      return false;
  }
  YAODAQ_API bool operator!=( const Component& comp ) const noexcept { return !( *this == comp ); }

private:
  Role m_role{ static_cast<Role>( 0 ) };
};

}  // namespace yaodaq
