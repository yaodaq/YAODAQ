#include "yaodaq/Server.hpp"

#include "yaodaq/ConnectionState.hpp"
#include "yaodaq/Exception.hpp"
#include "yaodaq/WebsocketCloseConstants.hpp"

#include <cstddef>
#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <memory>
#include <string>
#include <utility>

yaodaq::Server::Server( const std::string& name, const std::string& host, const int port, const int backlog, const std::size_t maxConnections, const int handshakeTimeoutSecs, const int addressFamily, const int pingIntervalSeconds, const std::string& type ) :
  ix::WebSocketServer( port, host, backlog, maxConnections, handshakeTimeoutSecs, addressFamily, pingIntervalSeconds ), m_identifier( Component::Name::Server, type, name ), JsonRPCHandler(), m_threadPool()
{
  ix::initNetSystem();
  this->setConnectionStateFactory( [this]() { return yaodaq::ConnectionState::createConnectionState(); } );
  this->setOnClientMessageCallback(
    [this]( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg )
    {
      if( msg->type == ix::WebSocketMessageType::Message ) { onMessage( connectionState, webSocket, msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Fragment ) { onFragment( connectionState, webSocket, msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Open )
      {
        checkClient( connectionState, webSocket, msg );
        onOpen( connectionState, webSocket, msg->openInfo.uri, msg->openInfo.headers, msg->openInfo.protocol );
      }
      else if( msg->type == ix::WebSocketMessageType::Close ) { onClose( connectionState, webSocket, msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote ); }
      else if( msg->type == ix::WebSocketMessageType::Error ) { onError( connectionState, webSocket, msg->errorInfo.retries, msg->errorInfo.wait_time, msg->errorInfo.http_status, msg->errorInfo.reason, msg->errorInfo.decompressionError ); }
      else if( msg->type == ix::WebSocketMessageType::Ping ) { onPing( connectionState, webSocket, msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Pong ) { onPong( connectionState, webSocket, msg->str, msg->wireSize, msg->binary ); }
    } );
  Add( "getNumberOfClients", GetHandle( &yaodaq::Server::getNumberOfClients, *this ) );
}

std::size_t yaodaq::Server::getNumberOfClients() noexcept { return getClients().size(); }

yaodaq::Server::~Server() noexcept
{
  stop();
  ix::uninitNetSystem();
}

bool yaodaq::Server::loop()
{
  listenAndStart();
  return 0;
}

void yaodaq::Server::checkClient( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg )
{
  if( msg->openInfo.headers["Yaodaq-Id"].empty() )
  {
    if( m_rejectBrowser ) webSocket.stop( WebSocketCloseConstant::NoYaodaqId, WebSocketCloseConstant::NoYaodaqIdMessage );
    else
      std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->setID( Identifier( Component::Name::Browser ) );
  }
  else
  {
    std::static_pointer_cast<yaodaq::ConnectionState>( connectionState )->setID( Identifier::createFromstring( msg->openInfo.headers["Yaodaq-Id"] ) );
  }
  std::cout << connectionState->getId() << std::endl;
}

void yaodaq::Server::onMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary )
{
  nlohmann::json message = nlohmann::json::parse( str, nullptr, false );
  if( !message.is_discarded() )
  {
    if( message.contains( "method" ) ) onJsonRPCRequest( connectionState, webSocket, message );
    else if( message.contains( "result" ) || message.contains( "error" ) ) { onJsonRPCResponse( connectionState, webSocket, message ); }
  }
  else
    std::cout << str << " " << size << " " << binary << std::endl;
}

void yaodaq::Server::onFragment( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary ) { std::cout << str << " " << size << " " << binary << std::endl; }

void yaodaq::Server::onOpen( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& uri, ix::WebSocketHttpHeaders& headers, const std::string& protocol )
{
  std::cout << uri << "**" << protocol << " " << headers["Yaodaq-Id"] << std::endl;
}

void yaodaq::Server::onClose( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::uint16_t code, const std::string& reason, bool remote ) { std::cout << code << " " << reason << " " << remote << std::endl; }

void yaodaq::Server::onError( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::uint32_t retries, const double wait_time, const int http_status, const std::string& reason, const bool decompressionError )
{
  std::cout << retries << " " << wait_time << " " << http_status << " " << reason << " " << decompressionError << std::endl;
  ;
}

void yaodaq::Server::onPing( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary ) { std::cout << str << " " << size << " " << binary << std::endl; }

void yaodaq::Server::onPong( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary ) { std::cout << str << " " << size << " " << binary << std::endl; }

std::string yaodaq::Server::Send( const std::string& request )
{
  for( std::set<std::shared_ptr<ix::WebSocket>>::const_iterator it = getClients().begin(); it != getClients().end(); ++it ) { ( *it )->sendUtf8Text( request.c_str() ); }
  return "";
}

void yaodaq::Server::send( const std::string& str )
{
  for( auto&& client: getClients() )
  {
    if( client.get() != reinterpret_cast<ix::WebSocket*>( this ) )  // simple raw pointer comparison
    {
      client->sendUtf8Text( str );
    }
  }
}

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
      client->sendUtf8Text( str );
    }
  }
}

void yaodaq::Server::onJsonRPCResponse( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json response )
{
  // Extract ID
  jsonrpccxx::id_type id;
  if( response["id"].is_string() ) id = response["id"].get<std::string>();
  else
    id = response["id"].get<int>();
  std::shared_ptr<ServerRequest> request;
  {
    std::lock_guard<std::mutex> lock( m_map_mutex );
    auto                        it = m_server_construct_response.find( id );
    if( it == m_server_construct_response.end() ) return;  // request not found
    request = it->second;
  }

  {
    std::lock_guard<std::mutex> lock( request->mtx );
    request->responses[connectionState->getId()] = std::move( response );
    request->received_responses++;
  }
  request->cv.notify_one();
}

void yaodaq::Server::onJsonRPCRequest( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json request )
{
  // Capture everything by value or reference safely
  auto task = [this, connectionState, &webSocket, request]() mutable
  {
    std::string ret                       = HandleRequest( request );  // Server handler request
    auto        answers                   = std::make_shared<ServerRequest>();
    answers->expected_responses           = getClients().size() - 1;
    answers->responses[m_identifier.id()] = nlohmann::json::parse( ret );

    // Extract ID
    jsonrpccxx::id_type id;
    if( request["id"].is_string() ) id = request["id"].get<std::string>();
    else
      id = request["id"].get<int>();

    {
      std::lock_guard<std::mutex> lock( m_map_mutex );
      m_server_construct_response[id] = answers;
    }

    sendExcept( request.dump(), webSocket );

    // Wait for all responses
    std::unique_lock<std::mutex> lock( answers->mtx );
    answers->cv.wait( lock,
                      [answers, this]
                      {
                        answers->print();
                        return answers->received_responses == getClients().size() - 1;
                      } );

    // Build JSON response
    nlohmann::json json;
    json["jsonrpc"] = "2.0";
    json["id"]      = request["id"];
    json["result"]  = nlohmann::json::array();
    for( auto& [clientId, response]: answers->responses )
    {
      response["id"] = clientId;
      response.erase( "jsonrpc" );
      json["result"].push_back( response );
    }

    sendTo( json.dump(), webSocket );

    {
      std::lock_guard<std::mutex> lock2( m_map_mutex );
      m_server_construct_response.erase( id );
    }
  };

  // Enqueue the task in the thread pool
  m_threadPool.enqueue( task );
}
