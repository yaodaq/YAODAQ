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
#include <ixwebsocket/IXProgressCallback.h>
#include <magic_enum/magic_enum.hpp>
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
  if( m_identifier.component().role() != Component::Role::Logger ) add_websocket_callback( [this]( const Log& msg ) noexcept { send( msg ); } );
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
        if( WebSocketCloseConstant::isRejected( msg->closeInfo.code ) ) { onReject( Reject( msg->closeInfo ) ); }
        else
        {
          onClose( Close( msg->closeInfo ) );
        }
        break;
      }
      case ix::WebSocketMessageType::Error:
      {
        onError( Error( msg->errorInfo ) );
        break;
      }
      case ix::WebSocketMessageType::Ping:
      {
        onPing( Ping( msg->str, msg->wireSize, msg->binary ) );
        break;
      }
      case ix::WebSocketMessageType::Pong:
      {
        onPong( Pong( msg->str, msg->wireSize, msg->binary ) );
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

void yaodaq::Client::onOpen( const Open& open )
{
  info( "Connected to {} (uri: {})\nheaders: {}\nprotocol: {}\n{}", open.url(), open.uri(), yaodaq::Formatter::format( open.payload()["headers"] ), open.protocol(), yaodaq::Formatter::format( open() ) );
  send( open );
}

void yaodaq::Client::onMessage( const std::string& str, const std::size_t size, const bool binary )
{
  nlohmann::json message = nlohmann::json::parse( str, nullptr, false );
  if( !message.is_discarded() ) { onJsonRPC( message ); }
}

void yaodaq::Client::onClose( const Close& close )
{
  //if(m_client.getReadyState() != ix::ReadyState::Closed && m_client.getReadyState() != ix::ReadyState::Closing)
  //{
  send( close );
  if( close.remote() ) warn( "closing by remote: {} ({})", close.reason(), close.code() );
  else
    info( "closing: {} ({})", close.reason(), close.code() );
  //}
}

void yaodaq::Client::onReject( const Reject& reject )
{
  while( m_client.isAutomaticReconnectionEnabled() ) m_client.disableAutomaticReconnection();  // don't try to reconnect dude I don't want you !
                                                                                               //if(m_client.getReadyState() != ix::ReadyState::Closed && m_client.getReadyState() != ix::ReadyState::Closing)
                                                                                               //{
  send( reject );
  if( reject.remote() ) error( "rejected by remote: {} ({})", reject.reason(), reject.code() );
  else
    error( "rejected: {} ({})", reject.reason(), reject.code() );
  //}
}

void yaodaq::Client::onError( const Error& err )
{
  error( "error {} ({}), retries: {}, waiting_time: {}{}", err.reason(), err.http_status(), err.retries(), err.wait_time(), err.decompression_error() ? " (decompression error)" : "" );
  send( err );
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
  else if( json.contains( "meta" ) && json["meta"].contains( "type" ) )
  {
    switch( magic_enum::enum_cast<Message::Type>( json["meta"]["type"].get<std::string_view>(), magic_enum::case_insensitive ).value() )
    {
      case Message::Type::Log:
      {
        onLog( json );
        break;
      }
      default:
      {
        break;
      }
    }
  }
}

//void yaodaq::Client::onFragment( const std::string& str, const std::size_t size, const bool binary ) { std::cout << str << " " << size << " " << binary << std::endl; }

void yaodaq::Client::onPing( const Ping& ping )
{
  send( ping );
  info( "received ping: {}", ping.message() );
}

void yaodaq::Client::onPong( const Pong& pong )
{
  send( pong );
  info( "received pong: {}", pong.message() );
}

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

void yaodaq::Client::send( const Message& message, const send_as as ) noexcept
{
  static const ix::OnProgressCallback callback{ [this]( int current, int total ) noexcept -> bool
                                                {
                                                  debug_without_websocket( "sent {}/{} ({}%)", current + 1, total, ( current + 1 ) * 100.0 / total );
                                                  return true;
                                                } };
  if( m_client.getReadyState() == ix::ReadyState::Closed || m_client.getReadyState() == ix::ReadyState::Closing ) return;
  ix::WebSocketSendInfo ret;
  switch( as )
  {
    case send_as::utf8:
    {
      ret = m_client.sendUtf8Text( message.dump(), callback );
      break;
    }
    case send_as::binary:
    {
      ret = m_client.sendBinary( message.dump(), callback );
      break;
    }
  }
  if( ret.success ) debug_without_websocket( "sent successful, payload: {}, wire_size:{}", ret.payloadSize, ret.wireSize );
  else
  {
    const std::string error_message = fmt::format( "Error sending message of type '{}', payload: {}, wire_size:{}{}", message.meta()["type"].get<std::string_view>(), ret.payloadSize, ret.wireSize, ret.compressionError ? " (compression error)" : "" );
    error_without_websocket( error_message );
    m_client.sendUtf8Text( Log( spdlog::details::log_msg( identifier().id(), spdlog::level::err, error_message ) ).dump(), callback );
  }
}
