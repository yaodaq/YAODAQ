#include "yaodaq/Server.hpp"

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
  info( "Stopping server" );
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
  auto        task = [this, request = std::move( request_copy )]() mutable
  {
    nlohmann::json r            = nlohmann::json::parse( request );
    std::string    ret          = HandleRequest( request );  // Server handler request
    auto           answers      = std::make_shared<ServerRequest>();
    answers->expected_responses = getNumberOfClients();

    // Extract ID

    jsonrpc::id_t id;
    if( r["id"].is_string() ) id = r["id"].get<std::string>();
    else
      id = r["id"].get<std::int64_t>();

    // prepare to wait for responses from the clients
    {
      std::lock_guard<std::mutex> lock( m_server_own_request );
      m_server_own_construct_response[id] = answers;
    }

    sendToAll( request.data() );

    // Wait for all responses
    std::unique_lock<std::mutex> lock( answers->mtx );
    answers->cv.wait_for( lock, std::chrono::seconds( 10 ), [answers, this] { return answers->received_responses == getNumberOfClients(); } );

    simdjson::builder::string_builder builder;
    builder.start_object();
    // "jsonrpc"
    builder.append_key_value( "jsonrpc", "2.0" );
    builder.append_comma();
    // "id"
    if( r["id"].is_string() ) builder.append_key_value( "id", r["id"].get<std::string>() );
    else
      builder.append_key_value( "id", r["id"].get<std::int64_t>() );
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

    std::cout << "GGGGGGGGGGGG*" << builder.view() << "*****" << std::endl;
    // Aend to th client that send the request
    Receive( builder.view() );

    // cleaning
    {
      std::lock_guard<std::mutex> lock2( m_server_own_request );
      m_server_own_construct_response.erase( id );
    }
  };

  // Enqueue the task in the thread pool
  m_threadPool.enqueue( task );
}

YAODAQ_API yaodaq::Server::Server( const ServerConfig& cfg, const std::string_view name, const std::string_view type ) :
  m_identifier( Component::Role::Server, type, name ), m_server( cfg.getPort(), cfg.getHost().data(), cfg.getBacklog(), cfg.getMaxConnections(), cfg.getHandshakeTimeoutSecs(), cfg.getAddressFamily(), cfg.getPingIntervalSeconds() ),
  Logging( { Component::Role::Server, type, name } )
{
  ix::initNetSystem();
  add_websocket_callback( [this]( const Log& msg ) noexcept { sendToLoggers( msg.dump() ); } );
  setVerbosity( cfg.getVerbosity() );
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
  info( "client {} at {} port {} connected to server:\n  {}", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), yaodaq::Formatter::format( open.payload() ) );
  sendExcept( open.dump(), webSocket );
}

void yaodaq::Server::onClose( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Close& close )
{
  if( close.remote() ) warn( "client {} at {} port {} disconnected to server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), close.reason(), close.code() );
  else
    warn( "client {} at {} port {} disconnected to server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), close.reason(), close.code() );
  sendExcept( close.dump(), webSocket );
}

void yaodaq::Server::onReject( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Reject& reject )
{
  if( reject.remote() ) error( "client {} at {} port {} rejected by server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), reject.reason(), reject.code() );
  else
    error( "client {} at {} port {} rejected by server: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), reject.reason(), reject.code() );
}

void yaodaq::Server::onMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary )
{
  thread_local simdjson::dom::parser  parser;  //to remove
  thread_local simdjson::dom::element r;
  r = parser.parse( str );  //to remove
  if( !r["method"].error() || !r["notification"].error() ) onJsonRPCRequest( connectionState, webSocket, parser, str );
  else if( !r["result"].error() || !r["error"].error() )
  {
    jsonrpc::id_t id;
    auto          id_elem = r["id"];
    if( id_elem.type() == simdjson::dom::element_type::STRING )
    {
      std::string_view s;
      id_elem.get( s );
      id = std::string( s );
    }
    else
    {
      int64_t v;
      id_elem.get( v );
      id = v;
    }
    onJsonRPCResponse( connectionState, webSocket, id, str );
  }
  else
  {
    yaodaq::Message::Type mess = magic_enum::enum_cast<yaodaq::Message::Type>( r["meta"]["type"].get<std::string_view>() ).value_or( yaodaq::Message::Type::Unknown );
    switch( mess )
    {
      case Message::Type::Log:
      {
        onLog( connectionState, webSocket, str );
        break;
      }
      default:
      {
        sendToAll( str );
        break;
      }
    }
  }
}

void yaodaq::Server::onLog( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string_view& str )
{
  // Log the message with the original level, logger, and source location
  sendToLoggers( str );
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
void yaodaq::Server::onJsonRPCRequest( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, simdjson::dom::parser& parser, const std::string_view& str )
{
  simdjson::dom::element r;
  r         = parser.parse( str );  //to remove
  // Capture everything by value or reference safely
  auto task = [this, connectionState, ws = &webSocket, r]() mutable
  {
    std::string str                  = simdjson::minify( r );
    std::string ret                  = HandleRequest( str );  // Server handler request
    auto        answers              = std::make_shared<ServerRequest>();
    answers->expected_responses      = getNumberOfClients() - 1;
    answers->responses[m_identifier] = ret;  // put the webserver response to the request
    // Extract ID
    jsonrpc::id_t id;
    auto          id_elem = r["id"];
    if( id_elem.type() == simdjson::dom::element_type::STRING )
    {
      std::string_view s;
      id_elem.get( s );
      id = std::string( s );
    }
    else
    {
      int64_t v;
      id_elem.get( v );
      id = v;
    }
    // prepare to wait for responses from the clients
    {
      std::lock_guard<std::mutex> lock( m_map_mutex );
      m_server_construct_response[id] = answers;
    }

    // send the request to the clients
    sendExcept( str, *ws );

    // Wait for all responses
    std::unique_lock<std::mutex> lock( answers->mtx );
    answers->cv.wait( lock, [answers, this] { return answers->received_responses == m_server.getClients().size() - 1; } );

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
      m_server_construct_response.erase( id );
    }
  };

  // Enqueue the task in the thread pool
  m_threadPool.enqueue( task );
}

// Server received a response
void yaodaq::Server::onJsonRPCResponse( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const jsonrpc::id_t& id, const std::string_view& str )
{
  std::unordered_map<jsonrpc::id_t, std::shared_ptr<yaodaq::ServerRequest>>::iterator it{ nullptr };
  bool                                                                                is_response_to_send_to_client{ false };
  bool                                                                                is_response_for_the_server{ false };

  std::shared_ptr<yaodaq::ServerRequest> request;

  {
    std::lock_guard<std::mutex> lock( m_map_mutex );
    it = m_server_construct_response.find( id );
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
      request->responses[std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID()] = str;
      request->received_responses++;
    }
    request->cv.notify_one();
  }
  else  // Maybe is a response for the server ? D the check
  {
    // The server did its own requests
    std::lock_guard<std::mutex> lock( m_server_own_request );
    it = m_server_own_construct_response.find( id );
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
      request->responses[std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID()] = str;
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

// Send to loggers
void yaodaq::Server::sendToLoggers( const std::string_view& str )
{
  std::string                 val{ str };
  std::lock_guard<std::mutex> lock( m_mutex );

  //auto it = m_clients.find( yaodaq::Component::Role::Logger );

  for( auto&& client: m_registry.get() )
  {
    if( client.first.component().role() == yaodaq::Component::Role::Logger ) { client.second->sendUtf8Text( val ); }
  }
}

// Send to all
void yaodaq::Server::sendToAll( const std::string_view& str ) noexcept
{
  for( auto&& client: m_server.getClients() ) { client->sendUtf8Text( std::string( str ) ); }
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
      sendToAll( Except( exception )().dump() );
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
      sendToAll( Except( exception )().dump() );
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
      sendToAll( Except( "exception in handleMessage" )().dump() );
    }
    catch( ... )
    {
    }
  }
}
