#pragma once
#include <cstdint>
#include <string>

namespace yaodaq
{

class Component
{
public:
  enum class Name : std::uint_least8_t
  {
    Server = 1,  //< A Yaodaq server
    Client,      //< A generic Yaodaq client
    Module,      //< A specific module within the Yaodaq system
    Board,       //< A board, presumably a hardware or logical component

    Browser = 100,  //< A client that is a browser (not a Yaodaq client)
  };
  Component() noexcept = delete;
  Component( const Component::Name& name ) noexcept : m_component( name ) {}
  std::string str() const;
  explicit    operator int() const noexcept { return static_cast<int>( m_component ); }
  explicit    operator Name() const noexcept { return static_cast<Name>( m_component ); }
  bool        operator==( const Component& comp ) const noexcept
  {
    if( static_cast<int>( m_component ) == static_cast<int>( comp ) ) return true;
    else
      return false;
  }
  bool operator!=( const Component& comp ) const noexcept { return !( ( *this ) == comp ); }

private:
  Name m_component{ static_cast<Name>( 0 ) };
};

}  // namespace yaodaq
