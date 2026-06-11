#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/ITransport.hpp"
#include "yaodaq/ThreadSafeQueue.hpp"

#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketSendData.h>

class WebSocket final : public yaodaq::ITransport
{
public:
  YAODAQ_API explicit WebSocket() : ITransport( "default", "Websocket" )
  {
    ix::initNetSystem();
    m_client.disablePerMessageDeflate();
    m_client.enablePong();
    m_client.setOnMessageCallback( [this]( const ix::WebSocketMessagePtr& msg ) noexcept { onMessage( msg ); } );
  }

  YAODAQ_API ~WebSocket() noexcept override
  {
    close();
    ix::uninitNetSystem();
  }

  YAODAQ_API bool open() final
  {
    m_client.setUrl( getParameters().get_as_f<std::string>( "url" ).or_throw( "url not provided !" ) );

    m_client.start();

    return true;
  }

  YAODAQ_API bool close() final
  {
    m_client.disableAutomaticReconnection();

    m_incoming.shutdown();

    m_client.close();

    return true;
  }

  YAODAQ_API void write( std::span<const std::byte> datd ) final { m_client.sendUtf8Text( ix::IXWebSocketSendData( reinterpret_cast<const char*>( datd.data() ), datd.size() ) ); }

  YAODAQ_API std::optional<std::vector<std::byte>> read() final { return m_incoming.pop(); }

  YAODAQ_API bool verifyParameters() final { return getParameters().contains( "url" ); }

private:
  ix::WebSocket m_client;

  ThreadSafeQueue<std::vector<std::byte>> m_incoming;

  void onMessage( const ix::WebSocketMessagePtr& msg ) noexcept
  {
    switch( msg->type )
    {
      case ix::WebSocketMessageType::Open:
      {
        info( "Connected to {} (uri:{}) protocol: {}", m_client.getUrl(), msg->openInfo.uri, msg->openInfo.protocol );
        break;
      }

      case ix::WebSocketMessageType::Close:
      {
        std::cout << "WebSocket closed\n";

        break;
      }

      case ix::WebSocketMessageType::Ping:
      {
        std::cout << "WebSocke ping\n";

        break;
      }
      case ix::WebSocketMessageType::Pong:
      {
        std::cout << "WebSocke pong\n";

        break;
      }
      case ix::WebSocketMessageType::Error:
      {
        std::cout << "WebSocket error: " << msg->errorInfo.reason << '\n';

        break;
      }

      case ix::WebSocketMessageType::Message:
      {
        m_incoming.push( std::vector<std::byte>( reinterpret_cast<const std::byte*>( msg->str.data() ), reinterpret_cast<const std::byte*>( msg->str.data() ) + msg->str.size() ) );

        break;
      }

      default:
      {
        break;
      }
    }
  }
};
