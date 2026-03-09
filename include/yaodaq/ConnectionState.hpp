#pragma once
#include "Identifier.hpp"
#include "yaodaq/Identifier.hpp"

#include <iostream>
#include <ixwebsocket/IXConnectionState.h>
namespace yaodaq
{

class ConnectionState : public ix::ConnectionState
{
public:
  ConnectionState() { _globalId--; };
  static std::shared_ptr<yaodaq::ConnectionState> createConnectionState() { return std::make_shared<yaodaq::ConnectionState>(); }
  virtual ~ConnectionState() = default;
  void setID( const Identifier& id )
  {
    if( static_cast<Component::Name>( id.component() ) == Component::Name::Browser )
    {
      m_identifier = Identifier( Component::Name::Browser, "Unknown", "Browser" + std::to_string( _globalId ) );
      computeId();
    }
    else
      m_identifier = id;
    _id = m_identifier.id();
  }
  Identifier getID() const noexcept { return m_identifier; }

private:
  virtual void               computeId() final { _globalId++; }
  virtual const std::string& getId() const final { return _id; }
  Identifier                 m_identifier{ static_cast<Component::Name>( 0 ) };
};

}  // namespace yaodaq
