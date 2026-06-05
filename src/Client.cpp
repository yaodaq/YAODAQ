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
#include <yaodaq/ICodec.hpp>

yaodaq::Client::~Client() noexcept
{
  m_client.stop();
  ix::uninitNetSystem();
}

yaodaq::Client::Client( const Identifier& id, const Config& config ) : m_identifier( id ), Logging( id )
{
  ix::initNetSystem();
  if( m_identifier.component().role() != Component::Role::Logger ) add_websocket_callback( [this]( const Log& msg ) noexcept { send( msg ); } );
  setVerbosity( config.getVerbosity() );
  m_client.setUrl( config.url() );
  if( config.isTLS() )
  {
    ix::SocketTLSOptions m_tlsOptions;
    m_tlsOptions.certFile = config.certFile();
    m_tlsOptions.keyFile  = config.keyFile();
    m_tlsOptions.caFile   = config.caFile();
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
    try
    {
      critical( "yaodaq::Exception: {}", exception.what() );
      send( Except( exception ) );
    }
    catch( ... )
    {
    }
  }
  catch( const std::exception& exception )
  {
    try
    {
      critical( "std::exception: {}", exception.what() );
      send( Except( exception ) );
    }
    catch( ... )
    {
    }
  }
  catch( ... )
  {
    try
    {
      critical( "exception in handleMessage" );
      send( Except( "exception in handleMessage" ) );
    }
    catch( ... )
    {
    }
  }
}

void yaodaq::Client::onOpen( const Open& open )
{
  info( "Connected (uri: {})\nheaders: {}\nprotocol: {}", open.uri(), yaodaq::Formatter::format( open.payload()["headers"] ), open.protocol() );
  send( open );
}

void yaodaq::Client::onMessage( const std::string& str, const std::size_t size, const bool binary )
{
  nlohmann::json json = nlohmann::json::parse( str, nullptr, false );
  Message        mess( json );
  //thread_local std::unique_ptr<ICodec> codec{ nullptr };
  //if( !codec ) codec = yaodaq::make_codec();
  // Message mess = codec->decode( str );
  switch( mess.type() )
  {
    case Message::Type::RPCRequest:
    {
      m_client.sendUtf8Text( HandleRequest( mess.payload().dump() ) );
      break;
    }
    case Message::Type::RPCResponse:
    {
      onResponse( mess.payload().dump() );
      break;
    }
    case Message::Type::Log:
    {
      onLog( mess() );
    }
  }
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
      info( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() );
      break;
    }
    case spdlog::level::level_enum::critical:
    {
      critical( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() );
      break;
    }
    case spdlog::level::level_enum::debug:
    {
      debug( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() );
      break;
    }
    case spdlog::level::level_enum::err:
    {
      error( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() );
      break;
    }
    case spdlog::level::level_enum::trace:
    {
      trace( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() );
      break;
    }
    case spdlog::level::level_enum::warn:
    {
      warn( "{}: {}", fmt::styled( json["payload"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["payload"]["message"].get<std::string>() );
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
      const auto raw = m_json_codec.encode( message );
      ret            = m_client.sendUtf8Text( ix::IXWebSocketSendData( reinterpret_cast<const char*>( raw.data() ), raw.size() ), callback );
      break;
    }
    case send_as::binary:
    {
      const auto raw = m_json_codec.encode( message );
      ret            = m_client.sendBinary( ix::IXWebSocketSendData( reinterpret_cast<const char*>( raw.data() ), raw.size() ), callback );
      break;
    }
  }
  if( ret.success ) debug_without_websocket( "sent successful, payload: {}, wire_size:{}", ret.payloadSize, ret.wireSize );
  else
  {
    const std::string error_message = fmt::format( "Error sending message of type '{}', payload: {}, wire_size:{}{}", message.meta()["type"].get<std::string_view>(), ret.payloadSize, ret.wireSize, ret.compressionError ? " (compression error)" : "" );
    error_without_websocket( error_message );
    const Log  log( spdlog::details::log_msg( identifier().id(), spdlog::level::err, error_message ) );
    const auto raw = m_json_codec.encode( log );
    m_client.sendUtf8Text( ix::IXWebSocketSendData( reinterpret_cast<const char*>( raw.data() ), raw.size() ), callback );
  }
}
