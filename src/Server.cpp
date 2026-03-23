#include "yaodaq/Server.hpp"

#include "yaodaq/ConnectionState.hpp"
#include "yaodaq/Exception.hpp"
#include "yaodaq/WebsocketCloseConstants.hpp"

#include <cstddef>
#include <cstdint>
#include <fmt/ranges.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <memory>
#include <string>
#include <string_view>

yaodaq::Server::~Server() noexcept
{
  stop();
  logger()->info( "server stopped" );
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
  auto task = [this, request]() mutable
  {
    std::string ret             = HandleRequest( request );  // Server handler request
    auto        answers         = std::make_shared<ServerRequest>();
    answers->expected_responses = getClients().size();

    // Extract ID
    nlohmann::json r = nlohmann::json::parse( request );
    jsonrpc::id_t  id;
    if( r["id"].is_string() ) id = r["id"].get<std::string>();
    else
      id = r["id"].get<std::int64_t>();

    // prepare to wait for responses from the clients
    {
      std::lock_guard<std::mutex> lock( m_server_own_request );
      m_server_own_construct_response[id] = answers;
    }

    sendToAll( std::string( request ) );

    // Wait for all responses
    std::unique_lock<std::mutex> lock( answers->mtx );
    answers->cv.wait( lock, [answers, this] { return answers->received_responses == getClients().size(); } );

    // Build JSON response
    nlohmann::json json;
    json["jsonrpc"] = "2.0";
    json["id"]      = r["id"];
    json["result"]  = nlohmann::json::array();
    for( auto& [clientId, response]: answers->responses )
    {
      response["yaodaq_id"] = clientId;
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

YAODAQ_API yaodaq::Server::Server( const std::string_view name,  //<! Name of the server
                                   const std::string_view host,  //<! Host of the server
                                   const std::uint16_t    port,  //<! port listen by the server
                                   const std::size_t maxConnections, const int handshakeTimeoutSecs, const int pingIntervalSeconds,
                                   const int backlog,  //<! maximum number of clients waiting to be connected
                                   const int addressFamily, const std::string_view type ) :
  m_identifier( Component::role::Server, type, name ), ix::WebSocketServer( port, std::string( host ), backlog, maxConnections, handshakeTimeoutSecs, addressFamily, pingIntervalSeconds ), Log( { Component::role::Server, type, name } )
{
  ix::initNetSystem();
  this->setConnectionStateFactory( [this]() { return yaodaq::ConnectionState::createConnectionState(); } );
  this->setOnClientMessageCallback(
    [this]( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg )
    {
      if( msg->type == ix::WebSocketMessageType::Message ) { onMessage( connectionState, webSocket, msg->str, msg->wireSize, msg->binary ); }
      //else if( msg->type == ix::WebSocketMessageType::Fragment ) { onFragment( connectionState, webSocket, msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Open )
      {
        checkClient( connectionState, webSocket, msg );
        onOpen( connectionState, webSocket, msg->openInfo.uri, msg->openInfo.headers, msg->openInfo.protocol );
      }
      else if( msg->type == ix::WebSocketMessageType::Close )
      {
        if( WebSocketCloseConstant::isRejected( msg->closeInfo.code ) ) { onReject( connectionState, webSocket, msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote ); }
        else
        {
          const std::string name = std::string( std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID().name() );
          {
            std::lock_guard<std::mutex> lock( m_mutex );
            m_clients.erase( name );
          }
          onClose( connectionState, webSocket, msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote );
        }
      }
      //else if( msg->type == ix::WebSocketMessageType::Error ) { onError( connectionState, webSocket, msg->errorInfo.retries, msg->errorInfo.wait_time, msg->errorInfo.http_status, msg->errorInfo.reason, msg->errorInfo.decompressionError ); }
      else if( msg->type == ix::WebSocketMessageType::Ping ) { onPing( connectionState, webSocket, msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Pong ) { onPong( connectionState, webSocket, msg->str, msg->wireSize, msg->binary ); }
    } );
  // Register procedure understood by the websocket server
  Add( "getNumberOfClients", GetHandle( &yaodaq::Server::getNumberOfClients, *this ) );
}

void yaodaq::Server::start()
{
  logger()->info( "server started at {}:{} ({}) with {} backlogs, {} maximum connections", getHost(), getPort(), ( getAddressFamily() == AF_INET ? "ip4" : getAddressFamily() == AF_INET6 ? "ip6" : "unknown" ), getBacklog(), getMaxConnections() );
  if( m_rejectBrowser ) logger()->warn( "All browsers will be kicked" );
  auto res = listenAndStart();
  if( !res ) throw Exception( "Error starting server" );
}

bool yaodaq::Server::loop()
{
  logger()->info( "server started at {}:{} ({}) with {} backlogs, {} maximum connections", getHost(), getPort(), ( getAddressFamily() == AF_INET ? "ip4" : getAddressFamily() == AF_INET6 ? "ip6" : "unknown" ), getBacklog(), getMaxConnections() );
  if( m_rejectBrowser ) logger()->warn( "All browsers will be kicked" );
  auto res = listenAndStart();
  if( !res ) throw Exception( "Error starting server" );
  wait();
  return 0;
}

void yaodaq::Server::checkClient( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg )
{
  if( msg->openInfo.headers["Yaodaq-Id"].empty() )
  {
    if( m_rejectBrowser ) { webSocket.stop( WebSocketCloseConstant::NoYaodaqId, WebSocketCloseConstant::NoYaodaqIdMessage ); }
    else
      std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->setBowserID();
  }
  else
  {
    std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->setID( Identifier::createFromstring( msg->openInfo.headers["Yaodaq-Id"] ) );
  }
  const std::string name = std::string( std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID().name() );
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    if( m_clients.count( name ) == 0 ) { m_clients.insert( name ); }
    else
    {
      webSocket.stop( WebSocketCloseConstant::ClientWithThisNameAlreadyConnected, WebSocketCloseConstant::ClientWithThisNameAlreadyConnectedMessage );
    }
  }
}

void yaodaq::Server::onOpen( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& uri, ix::WebSocketHttpHeaders& headers, const std::string& protocol )
{
  std::vector<std::string> items;
  items.reserve( headers.size() );
  for( const auto& kv: headers ) { items.push_back( fmt::format( "    {}: {}\n", kv.first, kv.second ) ); }
  std::string result = fmt::format( "headers:\n{}", fmt::join( items, "" ) );
  logger()->info( "client {} at {} port {} connected to server:\n  uri: {}\n  protocol: {}\n  {}", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), uri, protocol, result );
}

void yaodaq::Server::onClose( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::uint16_t code, const std::string& reason, bool remote )
{
  if( remote ) logger()->warn( "client {} at {} port {} disconnected to server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), reason, code );
  else
    logger()->warn( "client {} at {} port {} disconnected to server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), reason, code );
}

void yaodaq::Server::onReject( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::uint16_t code, const std::string& reason, bool remote )
{
  if( remote ) logger()->error( "client {} at {} port {} rejected by server remotelly: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), reason, code );
  else
    logger()->error( "client {} at {} port {} rejected by server: {}({})", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), reason, code );
}

void yaodaq::Server::onMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary )
{
  nlohmann::json message = nlohmann::json::parse( str, nullptr, false );
  if( !message.is_discarded() )
  {
    if( message.contains( "method" ) || message.contains( "notification" ) ) onJsonRPCRequest( connectionState, webSocket, message );
    else if( message.contains( "result" ) || message.contains( "error" ) )
      onJsonRPCResponse( connectionState, webSocket, message );
  }
  else
    std::cout << str << " " << size << " " << binary << std::endl;
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
  auto task = [this, connectionState, &webSocket, request]() mutable
  {
    std::string ret                       = HandleRequest( request );  // Server handler request
    auto        answers                   = std::make_shared<ServerRequest>();
    answers->expected_responses           = getClients().size() - 1;
    answers->responses[m_identifier.id()] = nlohmann::json::parse( ret );  // put the webserver response to the request

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
    sendExcept( request.dump(), webSocket );

    // Wait for all responses
    std::unique_lock<std::mutex> lock( answers->mtx );
    answers->cv.wait( lock, [answers, this] { return answers->received_responses == getClients().size() - 1; } );

    // Build JSON response
    nlohmann::json json;
    json["jsonrpc"] = "2.0";
    json["id"]      = request["id"];
    json["result"]  = nlohmann::json::array();
    for( auto& [clientId, response]: answers->responses )
    {
      response["yaodaq_id"] = clientId;
      response.erase( "jsonrpc" );
      json["result"].push_back( response );
    }

    // Aend to th client that send the request
    sendTo( json.dump(), webSocket );

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
      request->responses[connectionState->getId()] = std::move( response );
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
      request->responses[connectionState->getId()] = std::move( response );
      request->received_responses++;
    }
    request->cv.notify_one();
  }
}

std::size_t yaodaq::Server::getNumberOfClients() noexcept { return getClients().size(); }

//void yaodaq::Server::onFragment( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary ) { std::cout << str << " " << size << " " << binary << std::endl; }

//void yaodaq::Server::onError( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::uint32_t retries, const double wait_time, const int http_status, const std::string& reason, const bool decompressionError )
//{
//  std::cout << retries << " " << wait_time << " " << http_status << " " << reason << " " << decompressionError << std::endl;
// ;
//}

void yaodaq::Server::onPing( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary )
{ logger()->info( "client {} at {} port {} sent ping: {}", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), str ); }

void yaodaq::Server::onPong( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary )
{ logger()->info( "client {} at {} port {} sent pong: {}", connectionState->getId(), connectionState->getRemoteIp(), connectionState->getRemotePort(), str ); }

void yaodaq::Server::sendExcept( const std::string& str, ix::WebSocket& webSocket )
{
  for( auto&& client: getClients() )
  {
    if( client.get() != reinterpret_cast<ix::WebSocket*>( this ) && client.get() != &webSocket )  // simple raw pointer comparison
    {
      client->sendUtf8Text( str );
    }
  }
}

void yaodaq::Server::sendTo( const std::string& str, ix::WebSocket& webSocket )
{
  for( auto&& client: getClients() )
  {
    if( client.get() == &webSocket )  // simple raw pointer comparison
    {
      client->sendUtf8Text( str.c_str() );
    }
  }
}

// Send to all
void yaodaq::Server::sendToAll( const std::string& str )
{
  for( auto&& client: getClients() )
  {
    if( client.get() != reinterpret_cast<ix::WebSocket*>( this ) )  // simple raw pointer comparison
    {
      client->sendUtf8Text( str );
    }
  }
}
