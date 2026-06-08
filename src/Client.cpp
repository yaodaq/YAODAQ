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
  info( "Connected (uri: {})\nheaders: {}\nprotocol: {}", open.uri(), yaodaq::Formatter::format( open.headers() ), open.protocol() );
  send( open );
}

void yaodaq::Client::onMessage( const std::string& str, const std::size_t size, const bool binary )
{
  auto                     bytes = std::span{ reinterpret_cast<const std::byte*>( str.data() ), str.size() };
  std::unique_ptr<Message> msg   = m_json_codec.decode( bytes );
  switch( msg->type() )
  {
    case Message::Type::RPCRequest:
    {
      std::unique_ptr<RPCRequest> req( dynamic_cast<RPCRequest*>( msg.release() ) );
      m_client.sendUtf8Text( HandleRequest( { reinterpret_cast<const char*>( req->raw().data() ), req->raw().size() } ) );
      break;
    }
    case Message::Type::RPCResponse:
    {
      std::unique_ptr<RPCResponse> req( dynamic_cast<RPCResponse*>( msg.release() ) );
      onResponse( { reinterpret_cast<const char*>( req->raw().data() ), req->raw().size() } );
      break;
    }
    case Message::Type::Log:
    {
      std::unique_ptr<Log> req( dynamic_cast<Log*>( msg.release() ) );
      onLog( std::move( req ) );
      break;
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

void yaodaq::Client::onLog( const std::unique_ptr<Log> log )
{
  switch( static_cast<spdlog::level::level_enum>( log->level() ) )
  {
    case spdlog::level::level_enum::info:
    {
      info( "{}: {}", fmt::styled( log->name(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), log->message() );
      break;
    }
    case spdlog::level::level_enum::critical:
    {
      critical( "{}: {}", fmt::styled( log->name(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), log->message() );
      break;
    }
    case spdlog::level::level_enum::debug:
    {
      debug( "{}: {}", fmt::styled( log->name(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), log->message() );
      break;
    }
    case spdlog::level::level_enum::err:
    {
      error( "{}: {}", fmt::styled( log->name(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), log->message() );
      break;
    }
    case spdlog::level::level_enum::trace:
    {
      trace( "{}: {}", fmt::styled( log->name(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), log->message() );
      break;
    }
    case spdlog::level::level_enum::warn:
    {
      warn( "{}: {}", fmt::styled( log->name(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), log->message() );
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
    const std::string error_message = fmt::format( "Error sending message of type '{}', payload: {}, wire_size:{}{}", message.type_str(), ret.payloadSize, ret.wireSize, ret.compressionError ? " (compression error)" : "" );
    error_without_websocket( error_message );
    const Log  log( spdlog::details::log_msg( identifier().id(), spdlog::level::err, error_message ) );
    const auto raw = m_json_codec.encode( log );
    m_client.sendUtf8Text( ix::IXWebSocketSendData( reinterpret_cast<const char*>( raw.data() ), raw.size() ), callback );
  }
}
