#include "yaodaq/connector/websocket/Websocket.hpp"

#include <ixwebsocket/IXNetSystem.h>

yaodaq::connector::Websocket::Websocket() : Connector( "Websocket" ) { ix::initNetSystem(); }

yaodaq::connector::Websocket::~Websocket() noexcept
{
  m_client.stop();
  ix::uninitNetSystem();
}
