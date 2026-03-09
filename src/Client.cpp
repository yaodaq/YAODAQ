#include "yaodaq/Client.hpp"

#include "yaodaq/WebsocketCloseConstants.hpp"

#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <string>

yaodaq::Client::~Client() noexcept
{
  stop();
  ix::uninitNetSystem();
}

yaodaq::Client::Client( const std::string& name, const std::string& url, const int port, const std::string& type ) : m_identifier( Component::Name::Client, type, name ), JsonRPCHandler()
{
  ix::initNetSystem();
  setExtraHeaders( { { "Yaodaq-Id", m_identifier.id() } } );
  setUrl( url + ":" + std::to_string( port ) );
  setOnMessageCallback(
    [this]( const ix::WebSocketMessagePtr& msg )
    {
      if( msg->type == ix::WebSocketMessageType::Message ) { onMessage( msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Fragment ) { onFragment( msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Open ) { onOpen( msg->openInfo.uri, msg->openInfo.headers, msg->openInfo.protocol ); }
      else if( msg->type == ix::WebSocketMessageType::Close )
      {
        checkReject( msg->closeInfo );
        onClose( msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote );
      }
      else if( msg->type == ix::WebSocketMessageType::Error ) { onError( msg->errorInfo.retries, msg->errorInfo.wait_time, msg->errorInfo.http_status, msg->errorInfo.reason, msg->errorInfo.decompressionError ); }
      else if( msg->type == ix::WebSocketMessageType::Ping ) { onPing( msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Pong ) { onPong( msg->str, msg->wireSize, msg->binary ); }
    } );
}

void yaodaq::Client::checkReject( const ix::WebSocketCloseInfo& close_info )
{
  switch( close_info.code )
  {
    case yaodaq::WebSocketCloseConstant::NoYaodaqId:
      if( this->isAutomaticReconnectionEnabled() ) this->disableAutomaticReconnection();  // don't try to reconnect dude I don't want you !
      break;
    default: break;
  }
}

void yaodaq::Client::onMessage( const std::string& str, const std::size_t size, const bool binary )
{
  nlohmann::json message = nlohmann::json::parse( str, nullptr, false );
  if( !message.is_discarded() ) { onJsonRPC( message ); }
  else
    onText( str );
}

void yaodaq::Client::onFragment( const std::string& str, const std::size_t size, const bool binary ) { std::cout << str << " " << size << " " << binary << std::endl; }

void yaodaq::Client::onOpen( const std::string& uri, const ix::WebSocketHttpHeaders& headers, const std::string& protocol ) { std::cout << uri << " " << protocol << " " << std::endl; }

void yaodaq::Client::onClose( const std::uint16_t code, const std::string& reason, bool remote ) { std::cout << code << " " << reason << " " << remote << std::endl; }

void yaodaq::Client::onError( const std::uint32_t retries, const double wait_time, const int http_status, const std::string& reason, const bool decompressionError )
{
  std::cout << retries << " " << wait_time << " " << http_status << " " << reason << " " << decompressionError << std::endl;
}

void yaodaq::Client::onPing( const std::string& str, const std::size_t size, const bool binary ) { std::cout << str << " " << size << " " << binary << std::endl; }

void yaodaq::Client::onPong( const std::string& str, const std::size_t size, const bool binary ) { std::cout << str << " " << size << " " << binary << std::endl; }

std::string yaodaq::Client::Send( const std::string& request )
{
  nlohmann::json re = nlohmann::json::parse( request );

  // Extract ID
  jsonrpccxx::id_type id;
  if( re["id"].is_string() ) id = re["id"].get<std::string>();
  else
    id = re["id"].get<int>();

  std::promise<nlohmann::json> response;
  auto                         fut = response.get_future();

  {
    std::lock_guard<std::mutex> lock( m_mutex );
    m_responses[id] = std::move( response );
  }

  // Send request
  sendUtf8Text( request );

  // Wait for response with optional timeout
  //auto status = fut.wait_for(std::chrono::seconds(10));
  //if(status == std::future_status::timeout) {
  // Cleanup
  //    std::lock_guard<std::mutex> lock(m_mutex);
  //    m_responses.erase(id);
  //    throw std::runtime_error("JSON-RPC response timeout");
  // }

  nlohmann::json resp = fut.get();

  std::cout << "Received " << resp.dump() << std::endl;

  // Cleanup
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    m_responses.erase( id );
  }

  return resp.dump();
}

void yaodaq::Client::onJsonRPC( const nlohmann::json& json )
{
  if( json.contains( "result" ) || json.contains( "error" ) )
  {
    jsonrpccxx::id_type id;
    if( json["id"].is_string() ) id = json["id"].get<std::string>();
    else
      id = json["id"].get<int>();
    {
      auto                        it = m_responses.find( id );
      std::lock_guard<std::mutex> lock( m_mutex );
      if( it != m_responses.end() ) { it->second.set_value( json ); }
      else
      {
        std::cout << "[ResponseManager] Ignoring unknown ID " << json["id"] << "\n";
      }
    }
  }
  else if( json.contains( "method" ) ) { sendUtf8Text( HandleRequest( json ).c_str() ); }
  else
    std::cout << json.dump() << std::endl;
}

void yaodaq::Client::onText( const std::string& text ) { std::cout << text << std::endl; }

void yaodaq::Client::onJson( const nlohmann::json& json ) { std::cout << json << std::endl; }
