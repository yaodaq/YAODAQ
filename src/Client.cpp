#include "yaodaq/Client.hpp"

#include "yaodaq/Identifier.hpp"
#include "yaodaq/Message.hpp"
#include "yaodaq/Version.hpp"
#include "yaodaq/WebsocketCloseConstants.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <string>
#include <string_view>
#include <thread>

yaodaq::Client::~Client() noexcept
{
  stop();
  ix::uninitNetSystem();
}

yaodaq::Client::Client( const Identifier& id, const ClientConfig& client_config ) : m_identifier( id ), Logging( id ), m_url( client_config.url() )
{
  ix::initNetSystem();
  ix::WebSocket::setUrl( m_url );
  if( client_config.isTLS() )
  {
    ix::SocketTLSOptions m_tlsOptions;
    m_tlsOptions.certFile = client_config.certFile();
    m_tlsOptions.keyFile  = client_config.keyFile();
    m_tlsOptions.caFile   = client_config.caFile();
    m_tlsOptions.tls      = true;
    setTLSOptions( m_tlsOptions );
  }
  setExtraHeaders( { { "Yaodaq-Id", m_identifier.id() } } );
  setOnMessageCallback(
    [this]( const ix::WebSocketMessagePtr& msg )
    {
      if( msg->type == ix::WebSocketMessageType::Message ) { onMessage( msg->str, msg->wireSize, msg->binary ); }
      //else if( msg->type == ix::WebSocketMessageType::Fragment ) { onFragment( msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Open ) { onOpen( msg->openInfo.uri, msg->openInfo.headers, msg->openInfo.protocol ); }
      else if( msg->type == ix::WebSocketMessageType::Close )
      {
        if( WebSocketCloseConstant::isRejected( msg->closeInfo.code ) ) onReject( msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote );
        else
          onClose( msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote );
      }
      //else if( msg->type == ix::WebSocketMessageType::Error ) { onError( msg->errorInfo.retries, msg->errorInfo.wait_time, msg->errorInfo.http_status, msg->errorInfo.reason, msg->errorInfo.decompressionError ); }
      else if( msg->type == ix::WebSocketMessageType::Ping ) { onPing( msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Pong ) { onPong( msg->str, msg->wireSize, msg->binary ); }
    } );
  enableAutomaticReconnection();
  enablePerMessageDeflate();
  std::function<void( const spdlog::details::log_msg& msg )> callback = [this]( const spdlog::details::log_msg& msg ) { sendUtf8Text( Log( msg ).dump() ); };
  if( m_identifier.component().role() != Component::Role::Logger ) add_callback( callback );  // Avoid recursion infinity
}

void yaodaq::Client::onOpen( const std::string& uri, const ix::WebSocketHttpHeaders& headers, const std::string& protocol ) { logger()->info( "connected at {} with protocol {}", getUrl(), protocol ); }

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
  while( this->isAutomaticReconnectionEnabled() ) this->disableAutomaticReconnection();  // don't try to reconnect dude I don't want you !
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
  else if( json.contains( "method" ) || json.contains( "notification" ) ) { sendUtf8Text( HandleRequest( json ).c_str() ); }
  else if( json.contains( "yaodaq" ) && json["type"] == "Log" )
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
  logger()->log( static_cast<spdlog::level::level_enum>( json["content"]["level"] ),
                 fmt::format( "{}: {}", fmt::styled( json["content"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["content"]["payload"].get<std::string>() ) );
}

//void yaodaq::Client::onText( const std::string& text ) { std::cout << text << std::endl; }

//void yaodaq::Client::onJson( const nlohmann::json& json ) { std::cout << json << std::endl; }
