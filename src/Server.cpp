#include "yaodaq/Server.hpp"

#include "yaodaq/ConnectionState.hpp"
#include "yaodaq/Exception.hpp"
#include "yaodaq/Formatter.hpp"
#include "yaodaq/Message.hpp"
#include "yaodaq/MetaInfos.hpp"
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
void yaodaq::Server::Send( const std::string_view request )
{
  // Capture everything by value or reference safely
  std::string request_copy( request );
  auto        task = [this, request = std::move( request_copy )]() mutable
  {
    nlohmann::json r            = nlohmann::json::parse( request );
    std::string    ret          = HandleRequest( r );  // Server handler request
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

    // Build JSON response
    nlohmann::json json;
    json["jsonrpc"] = "2.0";
    json["id"]      = r["id"];
    json["result"]  = nlohmann::json::array();

    // Add server's own response first
    nlohmann::json server_response            = nlohmann::json::parse( ret );
    server_response["yaodaq_id"]["component"] = std::string( m_identifier.component() );
    server_response["yaodaq_id"]["type"]      = m_identifier.type();
    server_response["yaodaq_id"]["name"]      = m_identifier.name();
    server_response.erase( "jsonrpc" );
    json["result"].push_back( server_response );

    for( auto& [clientId, response]: answers->responses )
    {
      response["yaodaq_id"]["component"] = std::string( clientId.component() );
      response["yaodaq_id"]["type"]      = clientId.type();
      response["yaodaq_id"]["name"]      = clientId.name();
      response.erase( "jsonrpc" );
      json["result"].push_back( response );
    }
    // Aend to th client that send the request
    Receive( json.dump() );

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
  const std::string name = std::string( std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID().name() );
  {
    {
      std::lock_guard<std::mutex> lock( m_mutex );
      const Component::Role       role{ std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID().component().role() };
      if( m_clients[role].count( name ) == 0 ) { m_clients[role].emplace( name, std::ref( webSocket ) ); }
      else
      {
        webSocket.stop( WebSocketCloseConstant::ClientWithThisNameAlreadyConnected, WebSocketCloseConstant::ClientWithThisNameAlreadyConnectedMessage );
      }
    }
  }
}

void yaodaq::Server::onOpen( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const yaodaq::Open& open )
{
  info( "client {} at {} port {} connected to server:\n  {}", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), yaodaq::Formatter::format( open.payload() ) );
  sendToAll( open.dump() );
}

void yaodaq::Server::onClose( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Close& close )
{
  sendToAll( close.dump() );
  if( close.remote() ) warn( "client {} at {} port {} disconnected to server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), close.reason(), close.code() );
  else
    warn( "client {} at {} port {} disconnected to server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), close.reason(), close.code() );
}

void yaodaq::Server::onReject( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Reject& reject )
{
  if( reject.remote() ) error( "client {} at {} port {} rejected by server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), reject.reason(), reject.code() );
  else
    error( "client {} at {} port {} rejected by server: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), reject.reason(), reject.code() );
}

void yaodaq::Server::onMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary )
{
  thread_local std::unique_ptr<ICodec> codec{ nullptr };
  if( !codec ) codec = yaodaq::make_codec();
  Message mess = codec->decode( str );
  switch( mess.type() )
  {
    case Message::Type::RPCRequest:
    {
      onJsonRPCRequest( connectionState, webSocket, mess.payload() );
      break;
    }
    case Message::Type::RPCResponse:
    {
      onJsonRPCResponse( connectionState, webSocket, mess.payload() );
      break;
    }
    case Message::Type::Log:
    {
      onLog( connectionState, webSocket, mess() );
      break;
    }
    default:
    {
      sendToAll( mess.dump() );
      break;
    }
  }
}

void yaodaq::Server::onLog( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json request )
{
  // Log the message with the original level, logger, and source location
  sendToLoggers( request.dump() );
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
void yaodaq::Server::onJsonRPCRequest( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json request )
{
  // Capture everything by value or reference safely
  auto task = [this, connectionState, ws = &webSocket, request]() mutable
  {
    std::string ret                  = HandleRequest( request );  // Server handler request
    auto        answers              = std::make_shared<ServerRequest>();
    answers->expected_responses      = getNumberOfClients() - 1;
    answers->responses[m_identifier] = nlohmann::json::parse( ret );  // put the webserver response to the request

    // Extract ID
    jsonrpc::id_t id;
    if( request["id"].is_string() ) id = request["id"].get<std::string>();
    else
      id = request["id"].get<std::int64_t>();

    // prepare to wait for responses from the clients
    {
      std::lock_guard<std::mutex> lock( m_map_mutex );
      m_server_construct_response[id] = answers;
    }

    // send the request to the clients
    sendExcept( request.dump(), *ws );

    // Wait for all responses
    std::unique_lock<std::mutex> lock( answers->mtx );
    answers->cv.wait( lock, [answers, this] { return answers->received_responses == m_server.getClients().size() - 1; } );

    // Build JSON response
    nlohmann::json json;
    json["jsonrpc"] = "2.0";
    json["id"]      = request["id"];
    json["result"]  = nlohmann::json::array();
    for( auto& [clientId, response]: answers->responses )
    {
      response["yaodaq_id"]["component"] = std::string( clientId.component() );
      response["yaodaq_id"]["type"]      = clientId.type();
      response["yaodaq_id"]["name"]      = clientId.name();
      response.erase( "jsonrpc" );
      json["result"].push_back( response );
    }

    // Aend to th client that send the request
    sendTo( json.dump(), *ws );

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
void yaodaq::Server::onJsonRPCResponse( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json response )
{
  // Extract ID
  jsonrpc::id_t id;
  if( response["id"].is_string() ) id = response["id"].get<std::string>();
  else
    id = response["id"].get<std::int64_t>();

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
      request->responses[std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID()] = std::move( response );
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
      request->responses[std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID()] = std::move( response );
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
  std::lock_guard<std::mutex> lock( m_mutex );

  auto it = m_clients.find( yaodaq::Component::Role::Logger );
  if( it == m_clients.end() ) return;

  for( auto& client: it->second ) { client.second.get().sendUtf8Text( std::string( str ) ); }
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
          onClose( connectionState, webSocket, Close( msg->closeInfo ) );
          {
            std::lock_guard<std::mutex> lock( m_mutex );
            const auto                  cs = std::static_pointer_cast<yaodaq::ConnectionState>( connectionState );

            const auto role = cs->getID().component().role();
            auto       it   = m_clients.find( role );

            if( it != m_clients.end() ) { it->second.erase( std::string( cs->getID().name() ) ); }
          }
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
