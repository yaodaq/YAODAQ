#include "yaodaq/Server.hpp"

#include "yaodaq/ConnectionState.hpp"
#include "yaodaq/Exception.hpp"
#include "yaodaq/MetaInfos.hpp"
#include "yaodaq/WebsocketCloseConstants.hpp"

#include <cstddef>
#include <cstdint>
#include <fmt/color.h>
#include <fmt/ranges.h>
#include <iostream>
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
  m_identifier( Component::Role::Server, type, name ), ix::WebSocketServer( cfg.getPort(), cfg.getHost().data(), cfg.getBacklog(), cfg.getMaxConnections(), cfg.getHandshakeTimeoutSecs(), cfg.getAddressFamily(), cfg.getPingIntervalSeconds() ),
  Logging( { Component::Role::Server, type, name } )
{
  ix::initNetSystem();
  if( cfg.isTLS() )
  {
    ix::SocketTLSOptions m_tlsOptions;
    m_tlsOptions.certFile = cfg.certFile();
    m_tlsOptions.keyFile  = cfg.keyFile();
    m_tlsOptions.caFile   = cfg.caFile();
    m_tlsOptions.tls      = true;
    setTLSOptions( m_tlsOptions );
  }
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
          auto              cs   = std::static_pointer_cast<yaodaq::ConnectionState>( connectionState );
          const std::string name = std::string( cs->getID().name() );
          {
            std::lock_guard<std::mutex> lock( m_mutex );
            m_clients.erase( name );
            auto it = m_clientss.find( cs->getID().component().role() );
            if( it != m_clientss.end() )
            {
              auto& vec = it->second;
              vec.erase( std::remove_if( vec.begin(), vec.end(), [&]( std::reference_wrapper<ix::WebSocket> ref ) { return &ref.get() == &webSocket; } ), vec.end() );
            }
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
  Add( "set_state", GetHandle( &yaodaq::Server::getNumberOfClients, *this ) );
  std::function<void( const spdlog::details::log_msg& msg )> callback = [this]( const spdlog::details::log_msg& msg )
  {
    // Convert payload to std::string
    std::string payload( msg.payload.data(), msg.payload.size() );

    // Create JSON object
    nlohmann::json j;
    j["yaodaq"] = true;
    j["meta"]   = yaodaq::MetaInfos::raw();
    j["type"]   = "log";
    nlohmann::json rr;
    rr["logger_name"] = std::string( msg.logger_name.data(), msg.logger_name.size() );
    rr["level"]       = static_cast<int>( msg.level );  // or spdlog::level::to_string_view(msg.level)
    rr["payload"]     = payload;

    // Convert time to nanoseconds since epoch
    auto time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>( msg.time.time_since_epoch() ).count();
    rr["time"]   = time_ns;

    // Handle source location (check for null pointers if needed)
    nlohmann::json source_loc;
    if( msg.source.filename ) { source_loc["filename"] = std::string( msg.source.filename ); }
    if( msg.source.funcname ) { source_loc["funcname"] = std::string( msg.source.funcname ); }
    source_loc["line"] = msg.source.line;
    rr["source_loc"]   = source_loc;
    j["log"]           = rr;
    sendToLoggers( j.dump() );
  };
  add_callback( callback );
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
    {
      std::lock_guard<std::mutex> lock( m_mutex );
      if( m_clients.count( name ) == 0 )
      {
        m_clients.insert( name );
        m_clientss[std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->getID().component().role()].push_back( webSocket );
      }
      else
      {
        webSocket.stop( WebSocketCloseConstant::ClientWithThisNameAlreadyConnected, WebSocketCloseConstant::ClientWithThisNameAlreadyConnectedMessage );
      }
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
    else if( message.contains( "yaodaq" ) && message["type"] == "log" )
      onLog( connectionState, webSocket, message );
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
  auto task = [this, connectionState, &webSocket, request]() mutable
  {
    std::string ret                  = HandleRequest( request );  // Server handler request
    auto        answers              = std::make_shared<ServerRequest>();
    answers->expected_responses      = getClients().size() - 1;
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
      response["yaodaq_id"]["component"] = std::string( clientId.component() );
      response["yaodaq_id"]["type"]      = clientId.type();
      response["yaodaq_id"]["name"]      = clientId.name();
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

// Send to loggers
void yaodaq::Server::sendToLoggers( const std::string& str )
{
  for( auto&& client: m_clientss[yaodaq::Component::Role::Logger] ) { client.get().sendUtf8Text( str.c_str() ); }
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
