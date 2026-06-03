#pragma once

#include "ITransport.hpp"
#include "ThreadSafeQueue.hpp"
#include "nlohmann/json.hpp"

#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketSendData.h>

class WebSocket final : public ITransport
{
public:
  explicit WebSocket( nlohmann::json json ) : ITransport( std::move( json ) )
  {
    ix::initNetSystem();
    m_client.disablePerMessageDeflate();
    m_client.enablePong();
    m_client.setOnMessageCallback( [this]( const ix::WebSocketMessagePtr& msg ) noexcept { onMessage( msg ); } );
  }

  ~WebSocket() noexcept override
  {
    close();

    ix::uninitNetSystem();
  }

  bool open() final
  {
    m_client.setUrl( getParameters()["url"].get<std::string>() );

    m_client.start();

    return true;
  }

  bool close() final
  {
    m_client.disableAutomaticReconnection();

    m_incoming.shutdown();

    m_client.close();

    return true;
  }

  void write(std::span<const std::byte> datd ) final
  {
    m_client.sendUtf8Text( ix::IXWebSocketSendData( reinterpret_cast<const char*>( datd.data() ), datd.size() ) );
  }

  std::optional<std::vector<std::byte>> read() final { return m_incoming.pop(); }

  bool verifyParameters() final { return getParameters().contains( "url" ); }

private:
  ix::WebSocket m_client;

  ThreadSafeQueue<std::vector<std::byte>> m_incoming;

  void onMessage( const ix::WebSocketMessagePtr& msg ) noexcept
  {
    switch( msg->type )
    {
      case ix::WebSocketMessageType::Open:
      {
        std::cout << "WebSocket connected\n" << msg->openInfo.uri << " " << msg->openInfo.protocol << " ";

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
        auto* data = reinterpret_cast<const std::byte*>(msg->str.data());
        m_incoming.push(std::vector<std::byte>(data, data + msg->str.size()));

        break;
      }

      default:
      {
        break;
      }
    }
  }
};
