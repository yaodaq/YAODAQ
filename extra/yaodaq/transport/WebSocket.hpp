#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/Formatter.hpp"
#include "yaodaq/ITransport.hpp"
#include "yaodaq/ThreadSafeQueue.hpp"

#include <bit>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketSendData.h>
#include <optional>
#include <string>

class WebSocketTransport final : public yaodaq::ITransport
{
public:
  YAODAQ_API explicit WebSocketTransport() : ITransport( "default", "Websocket" )
  {
    trace( "WebSocketTransport() called" );
    ix::initNetSystem();
    m_client.disablePerMessageDeflate();
    m_client.enablePong();
    m_client.setOnMessageCallback( [this]( const ix::WebSocketMessagePtr& msg ) noexcept { onMessage( msg ); } );
  }

  YAODAQ_API ~WebSocketTransport() noexcept override
  {
    trace( "~WebSocketTransport() called" );
    close();
    ix::uninitNetSystem();
  }

  YAODAQ_API bool open() final
  {
    trace( "open() called" );
    m_client.setUrl( getParameters().get_as_f<std::string>( "url" ).or_throw( "url not provided !" ) );
    m_client.start();
    return true;
  }

  YAODAQ_API bool close() final
  {
    trace( "close() called" );
    m_incoming.shutdown();
    m_client.disableAutomaticReconnection();
    m_client.close();
    return true;
  }

  YAODAQ_API void write( std::span<const std::byte> raw ) override
  {
    m_client.sendUtf8Text( ix::IXWebSocketSendData( reinterpret_cast<const char*>( raw.data() ), raw.size() ) );  //NOSONAR
  }

  YAODAQ_API std::optional<std::vector<std::byte>> read() override { return m_incoming.pop(); }

  YAODAQ_API bool verifyParameters() final
  {
    debug( "verifyParameters() called" );
    return getParameters().contains( "url" );
  }

private:
  ix::WebSocket                           m_client;
  ThreadSafeQueue<std::vector<std::byte>> m_incoming;
  void                                    onMessage( const ix::WebSocketMessagePtr& msg ) noexcept  //NOSONAR
  {
    switch( msg->type )
    {
      case ix::WebSocketMessageType::Open:
      {
        debug( "Connected to {} (uri: {})\nheaders: {}\nprotocol: {}", m_client.getUrl(), msg->openInfo.uri, yaodaq::Formatter::format( msg->openInfo.headers ), msg->openInfo.protocol );
        break;
      }
      case ix::WebSocketMessageType::Close:
      {
        if( msg->closeInfo.remote ) debug( "Closed by remote: {} ({})", msg->closeInfo.reason, msg->closeInfo.code );
        else
          debug( "Closed: {} ({})", msg->closeInfo.reason, msg->closeInfo.code );
        break;
      }
      case ix::WebSocketMessageType::Ping:
      {
        trace( msg->str );
        break;
      }
      case ix::WebSocketMessageType::Pong:
      {
        trace( msg->str );
        break;
      }
      case ix::WebSocketMessageType::Error:
      {
        error( "error {} ({}), retries: {}, waiting_time: {}{}", msg->errorInfo.reason, msg->errorInfo.http_status, msg->errorInfo.retries, msg->errorInfo.wait_time, msg->errorInfo.decompressionError ? " (decompression error)" : "" );
        break;
      }
      case ix::WebSocketMessageType::Message:
      {
        trace( "Received {} bytes{}", msg->wireSize, msg->binary ? "binary" : "" );
        m_incoming.push( std::vector<std::byte>( reinterpret_cast<const std::byte*>( msg->str.data() ), reinterpret_cast<const std::byte*>( msg->str.data() ) + msg->str.size() ) );
        break;
      }
      default: break;
    }
  }
};
