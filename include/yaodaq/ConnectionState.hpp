#pragma once

#include "yaodaq/Component.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Identifier.hpp"

#include <ixwebsocket/IXConnectionState.h>
#include <memory>
#include <string>
#include <string_view>

namespace yaodaq
{

class ConnectionState : public ix::ConnectionState
{
public:
  YAODAQ_API                                                 ConnectionState() : m_identifier( Component::Role::Client, "Unknown", "Unknown" ) { _globalId--; };
  YAODAQ_API static std::shared_ptr<yaodaq::ConnectionState> createConnectionState() { return std::make_shared<yaodaq::ConnectionState>(); }
  YAODAQ_API virtual ~ConnectionState() = default;
  YAODAQ_API void setBowserID( std::string_view type = "Unknown" ) noexcept
  {
    m_identifier = Identifier( Component::Role::Browser, type, "Browser" + std::to_string( _globalId ) );
    _id          = m_identifier.id();
    computeId();
  }
  YAODAQ_API void setID( const Identifier& identifier ) noexcept
  {
    m_identifier = identifier;
    _id          = m_identifier.id();
  }
  YAODAQ_API Identifier getID() const noexcept { return m_identifier; }

private:
  virtual void               computeId() final { _globalId++; }
  virtual const std::string& getId() const final { return _id; }
  Identifier                 m_identifier;
};

}  // namespace yaodaq
