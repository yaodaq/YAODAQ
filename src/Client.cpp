#include "yaodaq/Client.hpp"

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
    if( msg->type == ix::WebSocketMessageType::Message ) { onMessage( msg->str, msg->wireSize, msg->binary ); }
    //else if( msg->type == ix::WebSocketMessageType::Fragment ) { onFragment( msg->str, msg->wireSize, msg->binary ); }
    else if( msg->type == ix::WebSocketMessageType::Open ) { onOpen( Open( msg->openInfo ) ); }
    else if( msg->type == ix::WebSocketMessageType::Close )
    {
      if( WebSocketCloseConstant::isRejected( msg->closeInfo.code ) ) onReject( msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote );
      else
        onClose( msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote );
    }
    //else if( msg->type == ix::WebSocketMessageType::Error ) { onError( msg->errorInfo.retries, msg->errorInfo.wait_time, msg->errorInfo.http_status, msg->errorInfo.reason, msg->errorInfo.decompressionError ); }
    else if( msg->type == ix::WebSocketMessageType::Ping ) { onPing( msg->str, msg->wireSize, msg->binary ); }
    else if( msg->type == ix::WebSocketMessageType::Pong ) { onPong( msg->str, msg->wireSize, msg->binary ); }
  }
  catch( const yaodaq::Exception& exception )
  {
    logger()->error( "yaodaq::Exception: {}", exception.what() );
    send( Except( exception ) );
  }
  catch( const std::exception& exception )
  {
    logger()->error( "std::exception: {}", exception.what() );
    send( Except( exception ) );
  }
  catch( ... )
  {
    logger()->error( "exception in handleMessage" );
    send( Except( "exception in handleMessage" ) );
  }
}

void yaodaq::Client::onOpen( const Open& open ) { logger()->info( "connected at {} headers: {}\nprotocol: {}", open.uri(), open.headers(), open.protocol() ); }

void yaodaq::Client::onMessage( const std::string& str, const std::size_t size, const bool binary )
{
  nlohmann::json message = nlohmann::json::parse( str, nullptr, false );
  if( !message.is_discarded() ) { onJsonRPC( message ); }
  //else
  //  onText( str );
}

void yaodaq::Client::onClose( const std::uint16_t code, const std::string& reason, bool remote )
{
  if( remote ) logger()->warn( "closing by remote: {} ({})", reason, code );
  else
    logger()->info( "closing: {} ({})", reason, code );
}

void yaodaq::Client::onReject( const std::uint16_t code, const std::string& reason, bool remote )
{
  while( m_client.isAutomaticReconnectionEnabled() ) m_client.disableAutomaticReconnection();  // don't try to reconnect dude I don't want you !
  if( remote ) logger()->error( "rejected by remote: {} ({})", reason, code );
  else
    logger()->error( "rejected: {} ({})", reason, code );
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

void yaodaq::Client::onPing( const std::string& str, const std::size_t size, const bool binary ) { logger()->info( "received ping: {}", str ); }

void yaodaq::Client::onPong( const std::string& str, const std::size_t size, const bool binary ) { logger()->info( "received pong: {}", str ); }

void yaodaq::Client::onLog( const nlohmann::json& json )
{
  logger()->log( static_cast<spdlog::level::level_enum>( json["payload"]["level"] ),
                 fmt::format( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() ) );
}

//void yaodaq::Client::onText( const std::string& text ) { std::cout << text << std::endl; }

//void yaodaq::Client::onJson( const nlohmann::json& json ) { std::cout << json << std::endl; }

void yaodaq::Client::send( const Message& msg, const bool callback ) noexcept
{
  ix::WebSocketSendInfo ret = m_client.sendUtf8Text( msg.dump(),
                                                     [this, callback]( const int current, const int total ) -> bool
                                                     {
                                                       if( callback ) { logger()->info( "Downloaded {} bytes out of {}", current, total ); }
                                                       return true;
                                                     } );
  if( callback )
  {
    if( ret.success ) logger()->info( "sent {} payloadSize {} wireSize {}", ret.payloadSize, ret.wireSize );
    else
      logger()->error( "error sending {}\npayloadSize {} wireSize {}{}", msg.dump(), ret.payloadSize, ret.wireSize, ret.compressionError ? " compression error" : "" );
  }
}
