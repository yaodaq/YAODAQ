#include "yaodaq/Client.hpp"

#include "yaodaq/Formatter.hpp"
#include "yaodaq/Identifier.hpp"
#include "yaodaq/Message.hpp"
#include "yaodaq/Version.hpp"
#include "yaodaq/WebsocketCloseConstants.hpp"

#include <chrono>
#include <cstdint>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <string>
#include <string_view>
#include <thread>

yaodaq::Client::~Client() noexcept
{
  m_client.stop();
  ix::uninitNetSystem();
}

yaodaq::Client::Client( const Identifier& id, const ClientConfig& client_config ) : m_identifier( id ), Logging( id )
{
  ix::initNetSystem();
  if( m_identifier.component().role() != Component::Role::Logger ) add_websocket_callback( [this]( const Log& msg ) noexcept { m_client.sendUtf8Text( msg.dump() ); } );
  m_client.setUrl( client_config.url() );
  if( client_config.isTLS() )
  {
    ix::SocketTLSOptions m_tlsOptions;
    m_tlsOptions.certFile = client_config.certFile();
    m_tlsOptions.keyFile  = client_config.keyFile();
    m_tlsOptions.caFile   = client_config.caFile();
    m_tlsOptions.tls      = true;
    m_client.setTLSOptions( m_tlsOptions );
  }
  m_client.setExtraHeaders( { { "Yaodaq-Id", m_identifier.id() } } );
  m_client.setOnMessageCallback( [this]( const ix::WebSocketMessagePtr& msg ) noexcept { handleMessage( msg ); } );
  m_client.enableAutomaticReconnection();
  m_client.enablePerMessageDeflate();
}

void yaodaq::Client::handleMessage( const ix::WebSocketMessagePtr& msg ) noexcept
{
  try
  {
    switch( msg->type )
    {
      case ix::WebSocketMessageType::Message:
      {
        onMessage( msg->str, msg->wireSize, msg->binary );
        break;
      }
      case ix::WebSocketMessageType::Fragment:
      {
        //onFragment( msg->str, msg->wireSize, msg->binary );
        break;
      }
      case ix::WebSocketMessageType::Open:
      {
        Open open( msg->openInfo );
        open.setWebsocketInfos( m_client );
        onOpen( open );
        break;
      }
      case ix::WebSocketMessageType::Close:
      {
        if( WebSocketCloseConstant::isRejected( msg->closeInfo.code ) ) onReject( msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote );
        else
          onClose( msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote );
        break;
      }
      case ix::WebSocketMessageType::Error:
      {
        //onError( msg->errorInfo.retries, msg->errorInfo.wait_time, msg->errorInfo.http_status, msg->errorInfo.reason, msg->errorInfo.decompressionError );
        break;
      }
      case ix::WebSocketMessageType::Ping:
      {
        onPing( msg->str, msg->wireSize, msg->binary );
        break;
      }
      case ix::WebSocketMessageType::Pong:
      {
        onPong( msg->str, msg->wireSize, msg->binary );
        break;
      }
    }
  }
  catch( const yaodaq::Exception& exception )
  {
    critical( "yaodaq::Exception: {}", exception.what() );
    send( Except( exception ) );
  }
  catch( const std::exception& exception )
  {
    critical( "std::exception: {}", exception.what() );
    send( Except( exception ) );
  }
  catch( ... )
  {
    critical( "exception in handleMessage" );
    send( Except( "exception in handleMessage" ) );
  }
}

void yaodaq::Client::onOpen( const Open& open ) { info( "Connected to {} (uri: {})\nheaders: {}\nprotocol: {}\n{}", open.url(), open.uri(), yaodaq::Formatter::format( open.payload()["headers"] ), open.protocol(), yaodaq::Formatter::format( open.dump() ) ); }

void yaodaq::Client::onMessage( const std::string& str, const std::size_t size, const bool binary )
{
  nlohmann::json message = nlohmann::json::parse( str, nullptr, false );
  if( !message.is_discarded() ) { onJsonRPC( message ); }
  //else
  //  onText( str );
}

void yaodaq::Client::onClose( const std::uint16_t code, const std::string& reason, bool remote )
{
  if( remote ) warn( "closing by remote: {} ({})", reason, code );
  else
    info( "closing: {} ({})", reason, code );
}

void yaodaq::Client::onReject( const std::uint16_t code, const std::string& reason, bool remote )
{
  while( m_client.isAutomaticReconnectionEnabled() ) m_client.disableAutomaticReconnection();  // don't try to reconnect dude I don't want you !
  if( remote ) error( "rejected by remote: {} ({})", reason, code );
  else
    error( "rejected: {} ({})", reason, code );
}

/**
 * @brief
 *
 * @param json
 * If is JSONRPC :
 * 1) If it contains method or notification we handle the request and send the result to the Websocket server
 * 2) If it's a result  or error we pass it to Received function to let the client aknoledge it
 **/
void yaodaq::Client::onJsonRPC( const nlohmann::json& json )
{
  if( json.contains( "result" ) || json.contains( "error" ) ) onResponse( json.dump() );
  else if( json.contains( "method" ) || json.contains( "notification" ) ) { m_client.sendUtf8Text( HandleRequest( json ).c_str() ); }
  else if( json.contains( "meta" ) && json["meta"]["type"] == "Log" )
    onLog( json );
}

//void yaodaq::Client::onFragment( const std::string& str, const std::size_t size, const bool binary ) { std::cout << str << " " << size << " " << binary << std::endl; }

//void yaodaq::Client::onError( const std::uint32_t retries, const double wait_time, const int http_status, const std::string& reason, const bool decompressionError )
//{
//  std::cout << retries << " " << wait_time << " " << http_status << " " << reason << " " << decompressionError << std::endl;
//}

void yaodaq::Client::onPing( const std::string& str, const std::size_t size, const bool binary ) { info( "received ping: {}", str ); }

void yaodaq::Client::onPong( const std::string& str, const std::size_t size, const bool binary ) { info( "received pong: {}", str ); }

void yaodaq::Client::onLog( const nlohmann::json& json )
{
  switch( static_cast<spdlog::level::level_enum>( json["payload"]["level"].get<int>() ) )
  {
    case spdlog::level::level_enum::info:
    {
      info( fmt::format( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() ) );
      break;
    }
    case spdlog::level::level_enum::critical:
    {
      critical( fmt::format( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() ) );
      break;
    }
    case spdlog::level::level_enum::debug:
    {
      debug( fmt::format( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() ) );
      break;
    }
    case spdlog::level::level_enum::err:
    {
      error( fmt::format( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() ) );
      break;
    }
    case spdlog::level::level_enum::trace:
    {
      trace( fmt::format( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() ) );
      break;
    }
    case spdlog::level::level_enum::warn:
    {
      warn( fmt::format( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() ) );
      break;
    }
    default: break;
  }
}

void yaodaq::Client::send( const Message& msg, const bool callback ) noexcept
{
  ix::WebSocketSendInfo ret = m_client.sendUtf8Text( msg.dump(),
                                                     [this, callback]( const int current, const int total ) -> bool
                                                     {
                                                       if( callback ) { info( "Downloaded {} bytes out of {}", current, total ); }
                                                       return true;
                                                     } );
  if( callback )
  {
    if( ret.success ) info( "sent {} payloadSize {} wireSize {}", ret.payloadSize, ret.wireSize );
    else
      error( "error sending {}\npayloadSize {} wireSize {}{}", msg.dump(), ret.payloadSize, ret.wireSize, ret.compressionError ? " compression error" : "" );
  }
}

void yaodaq::Client::send( const nlohmann::json& json ) { ix::WebSocketSendInfo ret = m_client.sendUtf8Text( json.dump() ); }
