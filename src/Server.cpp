#include "yaodaq/Server.hpp"

#include "yaodaq/Cleaner.hpp"
#include "yaodaq/ConnectionState.hpp"
#include "yaodaq/Exception.hpp"
#include "yaodaq/Formatter.hpp"
#include "yaodaq/Message.hpp"
#include "yaodaq/WebsocketCloseConstants.hpp"

#include <cstddef>
#include <cstdint>
#include <fmt/color.h>
#include <fmt/ranges.h>
#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <simdjson.h>
#include <string>
#include <string_view>

yaodaq::Server::~Server() noexcept
{
  Cleaner::instance().remove( this );
  debug( "~Server called" );
  m_server.stop();
  ix::uninitNetSystem();
}

/**
 * @brief
 *
 * @param request
 * When the server do it own request for clients to respond
 * 1) prepare answers
 * 2) send the request to all clients
 * 3) wait the merging
 * 4) call receive
 **/
void yaodaq::Server::Send( const std::string_view re )
{
  // Capture everything by value or reference safely
  std::string request_copy( re );
  auto        answers = std::make_shared<ServerRequest>();
  auto        task    = [this, request = std::move( request_copy ), answers]() mutable
  {
    try
    {
      thread_local simdjson::dom::parser parser;
      simdjson::dom::element             r   = parser.parse( request );
      std::string                        ret = HandleRequest( request );  // Server handler request
      answers->expected_responses            = getNumberOfClients();
      // Extract ID
      jsonrpc::id_t id;

      const auto id_elem = r["id"];

      if( id_elem.type() == simdjson::dom::element_type::STRING )
      {
        std::string_view s;
        auto             ret = id_elem.get( s );
        id                   = std::string( s );
      }
      else
      {
        int64_t v;
        auto    ret = id_elem.get( v );
        id          = v;
      }
      // prepare to wait for responses from the clients
      {
        std::lock_guard<std::mutex> lock( m_server_own_request );
        m_server_own_construct_response[id] = answers;
      }
      sendToAll( request.data() );

      // Wait for all responses
      std::unique_lock<std::mutex> lock( answers->mtx );
      answers->cv.wait_for( lock, std::chrono::seconds( 10 ), [&] { return answers->received_responses == getNumberOfClients(); } );
      simdjson::builder::string_builder builder;
      builder.start_object();
      // "jsonrpc"
      builder.append_key_value( "jsonrpc", "2.0" );
      builder.append_comma();
      // "id"
      if( std::holds_alternative<std::string>( id ) ) builder.append_key_value( "id", std::get<std::string>( id ) );
      else
        builder.append_key_value( "id", std::get<std::int64_t>( id ) );
      builder.append_comma();
      // array of answers
      builder.escape_and_append_with_quotes( "result" );
      builder.append_colon();
      builder.start_array();
      for( auto& [clientId, response]: answers->responses )
      {
        builder.append_raw( fmt::format( R"({{"result": {},"yaodaq_id":{{"component":"{}","type":"{}","name":"{}"}}}})", response, std::string( clientId.component() ), std::string( clientId.type() ), std::string( clientId.name() ) ) );
        builder.append_raw( "," );
      }
      builder.append_raw( fmt::format( R"({{"result": {},"yaodaq_id":{{"component":"{}","type":"{}","name":"{}"}}}})", ret, std::string( m_identifier.component() ), std::string( m_identifier.type() ), std::string( m_identifier.name() ) ) );
      builder.end_array();
      builder.end_object();

      // Aend to th client that send the request
      Receive( builder.view() );

      // cleaning
      {
        std::lock_guard<std::mutex> lock2( m_server_own_request );
        m_server_own_construct_response.erase( id );
      }
    }
    catch( ... )
    {
      std::lock_guard<std::mutex> lock( answers->mtx );
      answers->exceptions.push_back( std::current_exception() );
    }
  };

  // Enqueue the task in the thread pool
  m_threadPool.enqueue( std::move( task ) );
}

YAODAQ_API yaodaq::Server::Server( const ServerConfig& cfg, const std::string_view name, const std::string_view type ) :
  m_identifier( Component::Role::Server, type, name ), m_log( std::make_shared<Logging>( Identifier( Component::Role::Server, type, name ) ) ),
  m_server( cfg.getPort(), cfg.getHost().data(), cfg.getBacklog(), cfg.getMaxConnections(), cfg.getHandshakeTimeoutSecs(), cfg.getAddressFamily(), cfg.getPingIntervalSeconds() )
{
  Cleaner::instance().add( this );
  ix::initNetSystem();
  this->setLogger( m_log );
  m_log->add_websocket_callback( [this]( const Log& msg ) noexcept { sendToLoggers( msg ); } );
  m_log->setVerbosity( cfg.getVerbosity() );
  if( cfg.isTLS() )
  {
    ix::SocketTLSOptions m_tlsOptions;
    m_tlsOptions.certFile = cfg.certFile();
    m_tlsOptions.keyFile  = cfg.keyFile();
    m_tlsOptions.caFile   = cfg.caFile();
    m_tlsOptions.tls      = true;
    m_server.setTLSOptions( m_tlsOptions );
  }
  m_server.setConnectionStateFactory( [this]() { return yaodaq::ConnectionState::createConnectionState(); } );
  m_server.setOnClientMessageCallback( [this]( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg ) noexcept { handleMessage( connectionState, webSocket, msg ); } );
  // Register procedure understood by the websocket server
  Add( "getNumberOfClients", GetHandle( &Server::getNumberOfClients, *this ) );
  std::pair<bool, std::string> ret = m_server.listen();
  if( !ret.first ) { throw Exception( ret.second ); }
}

void yaodaq::Server::start()
{
  info( "server started at {}:{} ({}) with {} backlogs, {} maximum connections", m_server.getHost(), m_server.getPort(),
        ( m_server.getAddressFamily() == AF_INET    ? "ip4"
          : m_server.getAddressFamily() == AF_INET6 ? "ip6"
                                                    : "unknown" ),
        m_server.getBacklog(), m_server.getMaxConnections() );
  if( m_rejectBrowser ) warn( "All browsers will be kicked" );
  m_server.start();
}

bool yaodaq::Server::loop()
{
  start();
  m_server.wait();
  return 0;
}

void yaodaq::Server::checkClient( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg )
{
  if( msg->openInfo.headers["Yaodaq-Id"].empty() )
  {
    if( m_rejectBrowser ) { webSocket.stop( WebSocketCloseConstant::NoYaodaqId, WebSocketCloseConstant::NoYaodaqIdMessage ); }
    else
      std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->setBrowserID();
  }
  else
  {
    std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->setID( Identifier::createFromstring( msg->openInfo.headers["Yaodaq-Id"] ) );
  }
  if( !m_registry.add( std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID(), &webSocket ) )
  {
    webSocket.stop( WebSocketCloseConstant::ClientWithThisNameAlreadyConnected, WebSocketCloseConstant::ClientWithThisNameAlreadyConnectedMessage );
  }
}

void yaodaq::Server::onOpen( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const yaodaq::Open& open )
{
  info( "client {} at {} port {} connected to server:\n  {}", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), yaodaq::Formatter::format( open.headers() ) );
  sendExcept( open, webSocket );
}

void yaodaq::Server::onClose( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Close& close )
{
  if( close.remote() ) warn( "client {} at {} port {} disconnected to server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), close.reason(), close.code() );
  else
    warn( "client {} at {} port {} disconnected to server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), close.reason(), close.code() );
  sendExcept( close, webSocket );
}

void yaodaq::Server::onReject( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Reject& reject )
{
  if( reject.remote() ) error( "client {} at {} port {} rejected by server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), reject.reason(), reject.code() );
  else
    error( "client {} at {} port {} rejected by server: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), reject.reason(), reject.code() );
}

void yaodaq::Server::onMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary )
{
  std::span<const std::byte> m_bytes{ reinterpret_cast<const std::byte*>( str.data() ), str.size() };
  std::unique_ptr<Message>   mess = m_json_codec.decode( m_bytes );
  switch( mess->type() )
  {
    case Message::Type::RPCRequest:
    {
      std::unique_ptr<RPCRequest> req( dynamic_cast<RPCRequest*>( mess.release() ) );
      onJsonRPCRequest( connectionState, webSocket, std::move( req ) );
      break;
    }
    case Message::Type::RPCResponse:
    {
      std::unique_ptr<RPCResponse> req( dynamic_cast<RPCResponse*>( mess.release() ) );
      onJsonRPCResponse( connectionState, webSocket, std::move( req ) );
      break;
    }
    case Message::Type::Log:
    {
      std::unique_ptr<Log> req( dynamic_cast<Log*>( mess.release() ) );
      Log                  log( req->name(), req->level(), req->message(), req->logger_time(), req->file_name(), req->function_name(), req->line(), req->column() );
      onLog( connectionState, webSocket, log );
      break;
    }
  }
}

void yaodaq::Server::onLog( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Log& log )
{
  // Log the message with the original level, logger, and source location
  sendToLoggers( log );
}

/**
 * @brief
 *
 * @param connectionState
 * @param webSocket
 * @param request
 * If Websocket server receive a request :
 * 1) It handles it
 * 2) Send the request to all client except the client how sent the request
 * 3) Merge the request
 * 4) Send the merged responses to the client
 **/
void yaodaq::Server::onJsonRPCRequest( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, std::unique_ptr<RPCRequest> requ )
{
  auto answers = std::make_shared<ServerRequest>();
  auto re      = std::shared_ptr<RPCRequest>( std::move( requ ) );
  // Capture everything by value or reference safely
  auto task    = [this, connectionState, ws = &webSocket, re, answers]() mutable
  {
    try
    {
      std::string ret                  = HandleRequest( re->raw() );  // Server handler request
      answers->expected_responses      = getNumberOfClients() - 1;
      answers->responses[m_identifier] = ret;  // put the webserver response to the request
      {
        std::lock_guard<std::mutex> lock( m_map_mutex );
        m_server_construct_response[re->id()] = answers;
      }

      // send the request to the clients
      sendExcept( re->raw(), *ws );

      // Wait for all responses
      std::unique_lock<std::mutex> lock( answers->mtx );
      answers->cv.wait( lock, [answers, this] { return answers->received_responses == m_server.getClients().size() - 1; } );
      simdjson::builder::string_builder builder;
      builder.start_object();
      // "jsonrpc"
      builder.append_key_value( "jsonrpc", "2.0" );
      builder.append_comma();
      // "id"
      if( std::holds_alternative<std::string>( re->id() ) ) builder.append_key_value( "id", std::get<std::string>( re->id() ) );
      else
        builder.append_key_value( "id", std::get<std::int64_t>( re->id() ) );
      builder.append_comma();
      // array of answers
      builder.escape_and_append_with_quotes( "result" );
      builder.append_colon();
      builder.start_array();
      bool first = true;
      for( auto& [clientId, response]: answers->responses )
      {
        if( !first ) builder.append_raw( "," );
        first = false;
        builder.append_raw( fmt::format( R"({{"result": {},"yaodaq_id":{{"component":"{}","type":"{}","name":"{}"}}}})", std::string( response ), std::string( clientId.component() ), std::string( clientId.type() ), std::string( clientId.name() ) ) );
      }
      builder.end_array();
      builder.end_object();
      // Send to th client that send the request
      sendTo( builder.view(), *ws );

      // cleaning
      {
        std::lock_guard<std::mutex> lock2( m_map_mutex );
        m_server_construct_response.erase( re->id() );
      }
    }
    catch( ... )
    {
      std::lock_guard<std::mutex> lock( answers->mtx );
      answers->exceptions.push_back( std::current_exception() );
    }
  };

  // Enqueue the task in the thread pool
  m_threadPool.enqueue( task );
}

// Server received a response
void yaodaq::Server::onJsonRPCResponse( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, std::unique_ptr<RPCResponse> response )
{
  std::unordered_map<jsonrpc::id_t, std::shared_ptr<yaodaq::ServerRequest>>::iterator it{ nullptr };
  bool                                                                                is_response_to_send_to_client{ false };
  bool                                                                                is_response_for_the_server{ false };

  std::shared_ptr<yaodaq::ServerRequest> request;

  {
    std::lock_guard<std::mutex> lock( m_map_mutex );
    it = m_server_construct_response.find( response->id() );
    if( it != m_server_construct_response.end() )
    {
      request                       = it->second;
      is_response_to_send_to_client = true;
    }
  }

  if( is_response_to_send_to_client )
  {
    {
      std::lock_guard<std::mutex> lock( request->mtx );
      request->responses[std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID()] = response->raw();
      request->received_responses++;
    }
    request->cv.notify_one();
  }
  else  // Maybe is a response for the server ? D the check
  {
    // The server did its own requests
    std::lock_guard<std::mutex> lock( m_server_own_request );
    it = m_server_own_construct_response.find( response->id() );
    if( it != m_server_own_construct_response.end() )
    {
      request                    = it->second;
      is_response_for_the_server = true;
    }
  }

  if( is_response_for_the_server )
  {
    {
      std::lock_guard<std::mutex> lock( request->mtx );
      request->responses[std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID()] = response->raw();
      request->received_responses++;
    }
    request->cv.notify_one();
  }
}

std::size_t yaodaq::Server::getNumberOfClients() noexcept
{
  std::lock_guard<std::mutex> lock( m_mutex );
  return m_server.getClients().size();
}

void yaodaq::Server::onPing( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Ping& ping )
{ info( "client {} at {} port {} sent ping: {}", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), ping.message() ); }

void yaodaq::Server::onError( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Error& error ) {}

void yaodaq::Server::onPong( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Pong& pong )
{ info( "client {} at {} port {} sent pong: {}", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), pong.message() ); }

void yaodaq::Server::sendExcept( const std::string_view& str, ix::WebSocket& webSocket )
{
  for( auto&& client: m_server.getClients() )
  {
    if( client.get() != &webSocket ) { client->sendUtf8Text( std::string( str ) ); }
  }
}

void yaodaq::Server::sendTo( const std::string_view& str, ix::WebSocket& webSocket )
{
  for( auto&& client: m_server.getClients() )
  {
    if( client.get() == &webSocket )  // simple raw pointer comparison
    {
      client->sendUtf8Text( std::string( str ) );
    }
  }
}

// Send to all
void yaodaq::Server::sendToAll( const std::string_view& message ) noexcept
{
  for( auto&& client: m_server.getClients() ) { client->sendUtf8Text( std::string( message ) ); }
}

void yaodaq::Server::sendExcept( const Message& message, ix::WebSocket& webSocket )
{
  const auto raw = m_json_codec.encode( message );
  for( auto&& client: m_server.getClients() )
  {
    if( client.get() != &webSocket ) { client->sendUtf8Text( ix::IXWebSocketSendData( reinterpret_cast<const char*>( raw.data() ), raw.size() ) ); }
  }
}

void yaodaq::Server::sendTo( const Message& message, ix::WebSocket& webSocket )
{
  const auto raw = m_json_codec.encode( message );
  for( auto&& client: m_server.getClients() )
  {
    if( client.get() == &webSocket )  // simple raw pointer comparison
    {
      client->sendUtf8Text( ix::IXWebSocketSendData( reinterpret_cast<const char*>( raw.data() ), raw.size() ) );
    }
  }
}
// Send to all
void yaodaq::Server::sendToAll( const Message& message ) noexcept
{
  const auto raw = m_json_codec.encode( message );
  for( auto&& client: m_server.getClients() ) { client->sendUtf8Text( ix::IXWebSocketSendData( reinterpret_cast<const char*>( raw.data() ), raw.size() ) ); }
}

// Send to loggers
void yaodaq::Server::sendToLoggers( const Message& message )
{
  const auto                  raw = m_json_codec.encode( message );
  std::lock_guard<std::mutex> lock( m_mutex );
  for( auto&& client: m_registry.get() )
  {
    if( client.first.component().role() == yaodaq::Component::Role::Logger ) { client.second->sendUtf8Text( ix::IXWebSocketSendData( reinterpret_cast<const char*>( raw.data() ), raw.size() ) ); }
  }
}

void yaodaq::Server::handleMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg ) noexcept
{
  try
  {
    switch( msg->type )
    {
      case ix::WebSocketMessageType::Message:
      {
        onMessage( connectionState, webSocket, msg->str, msg->wireSize, msg->binary );
        break;
      }
      case ix::WebSocketMessageType::Fragment:
      {
        //onFragment( connectionState, webSocket, msg->str, msg->wireSize, msg->binary );
        break;
      }
      case ix::WebSocketMessageType::Open:
      {
        checkClient( connectionState, webSocket, msg );
        onOpen( connectionState, webSocket, Open( msg->openInfo ) );
        break;
      }
      case ix::WebSocketMessageType::Close:
      {
        if( WebSocketCloseConstant::isRejected( msg->closeInfo.code ) ) { onReject( connectionState, webSocket, Reject( msg->closeInfo ) ); }
        else
        {
          m_registry.erase( std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID() );
          onClose( connectionState, webSocket, Close( msg->closeInfo ) );
        }
        break;
      }
      case ix::WebSocketMessageType::Error:
      {
        onError( connectionState, webSocket, Error( msg->errorInfo ) );
        break;
      }
      case ix::WebSocketMessageType::Ping:
      {
        onPing( connectionState, webSocket, Ping( msg->str, msg->wireSize, msg->binary ) );
        break;
      }
      case ix::WebSocketMessageType::Pong:
      {
        onPong( connectionState, webSocket, Pong( msg->str, msg->wireSize, msg->binary ) );
        break;
      }
    }
  }
  catch( const yaodaq::Exception& exception )
  {
    try
    {
      critical( "yaodaq::Exception: {}", exception.what() );
      sendToAll( Except( exception ) );
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
      sendToAll( Except( exception ) );
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
      sendToAll( Except( "exception in handleMessage" ) );
    }
    catch( ... )
    {
    }
  }
}
