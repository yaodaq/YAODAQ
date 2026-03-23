#pragma once
#include <ixwebsocket/IXWebSocket.h>
#include <yaodaq/Connector.hpp>
#include <yaodaq/Export.hpp>

namespace yaodaq
{

namespace connector
{

class Websocket : public Connector
{
public:
  explicit Websocket();
  virtual ~Websocket() noexcept;
  virtual void connect() final {}
  virtual void disconnect() final {}

private:
  ix::WebSocket m_client;
};

}  // namespace connector

}  // namespace yaodaq
