#include "yaodaq/Client.hpp"

#include "yaodaq/Identifier.hpp"
#include "yaodaq/WebsocketCloseConstants.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <string>
#include <string_view>
#include <thread>

yaodaq::Client::~Client() noexcept
{
  while( getReadyState() == ix::ReadyState::Closing ) std::this_thread::sleep_for( std::chrono::microseconds( 100 ) );
  stop();
  ix::uninitNetSystem();
}

yaodaq::Client::Client( const Identifier& id, const ClientConfig& client_config ) : m_identifier( id ), Log( id )
{
  ix::initNetSystem();
  setExtraHeaders( { { "Yaodaq-Id", m_identifier.id() } } );
  setOnMessageCallback(
    [this]( const ix::WebSocketMessagePtr& msg )
    {
      if( msg->type == ix::WebSocketMessageType::Message ) { onMessage( msg->str, msg->wireSize, msg->binary ); }
      //else if( msg->type == ix::WebSocketMessageType::Fragment ) { onFragment( msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Open ) { onOpen( msg->openInfo.uri, msg->openInfo.headers, msg->openInfo.protocol ); }
      else if( msg->type == ix::WebSocketMessageType::Close )
      {
        if( WebSocketCloseConstant::isRejected( msg->closeInfo.code ) ) onReject( msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote );
        else
          onClose( msg->closeInfo.code, msg->closeInfo.reason, msg->closeInfo.remote );
      }
      //else if( msg->type == ix::WebSocketMessageType::Error ) { onError( msg->errorInfo.retries, msg->errorInfo.wait_time, msg->errorInfo.http_status, msg->errorInfo.reason, msg->errorInfo.decompressionError ); }
      else if( msg->type == ix::WebSocketMessageType::Ping ) { onPing( msg->str, msg->wireSize, msg->binary ); }
      else if( msg->type == ix::WebSocketMessageType::Pong ) { onPong( msg->str, msg->wireSize, msg->binary ); }
    } );
  setURL( client_config.getHost(), client_config.getPort(), client_config.getPath() );
  enableAutomaticReconnection();
  enablePerMessageDeflate();
  std::function<void( const spdlog::details::log_msg& msg )> callback = [this]( const spdlog::details::log_msg& msg )
  {
    // Convert payload to std::string
    std::string payload( msg.payload.data(), msg.payload.size() );

    // Create JSON object
    nlohmann::json j;
    j["yaodaq"] = true;
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
    sendUtf8Text( j.dump() );
    // Print JSON
    //std::cout << j.dump(2) << std::endl;
  };
  if( m_identifier.component().role() != Component::Role::Logger ) add_callback( callback );
}

void yaodaq::Client::onOpen( const std::string& uri, const ix::WebSocketHttpHeaders& headers, const std::string& protocol ) { logger()->info( "connected at {} with protocol {}", getUrl(), protocol ); }

void yaodaq::Client::onMessage( const std::string& str, const std::size_t size, const bool binary )
{
  nlohmann::json message = nlohmann::json::parse( str, nullptr, false );
  if( !message.is_discarded() ) { onJsonRPC( message ); }
  //else
  //  onText( str );
}

void yaodaq::Client::onClose( const std::uint16_t code, const std::string& reason, bool remote )
{
  if( remote ) logger()->warn( "closing by remote: {} ({})", reason, code );
  else
    logger()->info( "closing: {} ({})", reason, code );
}

void yaodaq::Client::onReject( const std::uint16_t code, const std::string& reason, bool remote )
{
  while( this->isAutomaticReconnectionEnabled() ) this->disableAutomaticReconnection();  // don't try to reconnect dude I don't want you !
  if( remote ) logger()->error( "rejected by remote: {} ({})", reason, code );
  else
    logger()->error( "rejected: {} ({})", reason, code );
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
  else if( json.contains( "method" ) || json.contains( "notification" ) ) { sendUtf8Text( HandleRequest( json ).c_str() ); }
  else if( json.contains( "yaodaq" ) && json["type"] == "log" )
    onLog( json );
}

//void yaodaq::Client::onFragment( const std::string& str, const std::size_t size, const bool binary ) { std::cout << str << " " << size << " " << binary << std::endl; }

//void yaodaq::Client::onError( const std::uint32_t retries, const double wait_time, const int http_status, const std::string& reason, const bool decompressionError )
//{
//  std::cout << retries << " " << wait_time << " " << http_status << " " << reason << " " << decompressionError << std::endl;
//}

void yaodaq::Client::onPing( const std::string& str, const std::size_t size, const bool binary ) { logger()->info( "received ping: {}", str ); }

void yaodaq::Client::onPong( const std::string& str, const std::size_t size, const bool binary ) { logger()->info( "received pong: {}", str ); }

void yaodaq::Client::onLog( const nlohmann::json& json )
{
  logger()->log( static_cast<spdlog::level::level_enum>( json["log"]["level"] ),
                 fmt::format( "{}: {}", fmt::styled( json["log"]["logger_name"].get<std::string>(), fmt::fg( fmt::color::gray ) | fmt::emphasis::bold ), json["log"]["payload"].get<std::string>() )  // Log the original payload
  );
}

//void yaodaq::Client::onText( const std::string& text ) { std::cout << text << std::endl; }

//void yaodaq::Client::onJson( const nlohmann::json& json ) { std::cout << json << std::endl; }
