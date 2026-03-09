#pragma once
#include "yaodaq/JsonRPCHandler.hpp"

#include <cstddef>
#include <ixwebsocket/IXWebSocketServer.h>
#include <string>
#include <yaodaq/Identifier.hpp>
#include <yaodaq/ThreadPool.hpp>
#include <yaodaq/Export.hpp>
namespace yaodaq
{

class Server : public ix::WebSocketServer, public JsonRPCHandler
{
public:
  YAODAQ_API explicit Server() noexcept = delete;
  YAODAQ_API Server( const std::string& name, const std::string& host, const int port = SocketServer::kDefaultPort, const int backlog = SocketServer::kDefaultTcpBacklog, const std::size_t maxConnections = SocketServer::kDefaultMaxConnections,
          const int handshakeTimeoutSecs = WebSocketServer::kDefaultHandShakeTimeoutSecs, const int addressFamily = SocketServer::kDefaultAddressFamily, const int pingIntervalSeconds = -1, const std::string& type = "YAODAQ" );
  YAODAQ_API void setTLS( const std::string& certFile, const std::string& keyFile, const std::string& caFile = "SYSTEM" )
  {
    m_tlsOptions.tls      = true;
    m_tlsOptions.certFile = certFile;
    m_tlsOptions.keyFile  = keyFile;
    m_tlsOptions.caFile   = caFile;
    setTLSOptions( m_tlsOptions );
  }
  YAODAQ_API bool loop();
  YAODAQ_API void send( const std::string& str );
  YAODAQ_API void rejectBrowsers() noexcept { m_rejectBrowser = true; }
  //void listen();
  YAODAQ_API virtual ~Server() noexcept;
  YAODAQ_API std::size_t       getNumberOfClients() noexcept;
  YAODAQ_API const Identifier& identifier() const noexcept { return m_identifier; }

private:
  ix::SocketTLSOptions m_tlsOptions;
  /* Check if client has all to be an yaodaq one and if he has the authorizations */
  void                 checkClient( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg );
  Identifier           m_identifier{ Component::Name::Server };
  bool                 m_rejectBrowser{ false };  //< Reject the Browsers

  void onMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary );
  void onFragment( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary );
  void onOpen( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& uri, ix::WebSocketHttpHeaders& headers, const std::string& protocol );
  void onClose( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::uint16_t code, const std::string& reason, bool remote );
  void onError( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::uint32_t retries, const double wait_time, const int http_status, const std::string& reason, const bool decompressionError );
  void onPing( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary );
  void onPong( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary );

  void onJsonRPCResponse( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json response );
  void onJsonRPCRequest( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json request );

  virtual std::string Send( const std::string& request ) override final;
  void                sendExcept( const std::string& str, ix::WebSocket& webSocket );
  void                sendTo( const std::string& str, ix::WebSocket& webSocket );

  ThreadPool m_threadPool;
};

}  // namespace yaodaq
