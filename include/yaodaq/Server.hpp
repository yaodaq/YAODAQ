#pragma once
#include "yaodaq/Defaults.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Identifier.hpp"
#include "yaodaq/JsonRPCHandler.hpp"
#include "yaodaq/Log.hpp"
#include "yaodaq/ThreadPool.hpp"

#include <cstddef>
#include <cstdint>
#include <ixwebsocket/IXWebSocketServer.h>
#include <mutex>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace yaodaq
{

struct ServerRequest
{
  std::mutex                                      mtx;  // protect shared state
  std::condition_variable                         cv;
  std::unordered_map<std::string, nlohmann::json> responses;
  size_t                                          expected_responses{ 0 };
  size_t                                          received_responses{ 0 };
};

class Server : public ix::WebSocketServer, public JsonRPCHandler, public Log
{
public:
  YAODAQ_API ~Server() noexcept;
  YAODAQ_API Server( const std::string_view name,                        //<! Name of the server
                     const std::string_view host      = defaults::host,  //<! Host of the server
                     const std::uint16_t    port      = defaults::port,  //<! port listen by the server
                     const std::size_t maxConnections = defaults::max_connections, const int handshakeTimeoutSecs = defaults::handshake_timeout, const int pingIntervalSeconds = defaults::ping_interval_seconds,
                     const int backlog       = defaults::backlog,  //<! maximum number of clients waiting to be connected
                     const int addressFamily = defaults::ip6 ? AF_INET6 : AF_INET, const std::string_view type = "YAODAQ" );

  YAODAQ_API const Identifier& identifier() const noexcept { return m_identifier; }

  YAODAQ_API void start();

  YAODAQ_API bool loop();

  YAODAQ_API void rejectBrowsers() noexcept { m_rejectBrowser = true; }

  /*YAODAQ_API void setTLS( const std::string& certFile, const std::string& keyFile, const std::string& caFile = "SYSTEM" )
  {
    m_tlsOptions.tls      = true;
    m_tlsOptions.certFile = certFile;
    m_tlsOptions.keyFile  = keyFile;
    m_tlsOptions.caFile   = caFile;
    setTLSOptions( m_tlsOptions );
  }*/

  YAODAQ_API std::size_t getNumberOfClients() noexcept;

private:
  YAODAQ_API explicit Server() noexcept = delete;
  Identifier m_identifier;
  ThreadPool m_threadPool;
  bool       m_ssl{ false };
  bool       m_rejectBrowser{ false };  //< Reject the Browsers
  //ix::SocketTLSOptions m_tlsOptions;

  virtual void Send( const std::string_view request ) override final;
  /* Check if client has all to be an yaodaq one and if he has the authorizations */
  void         checkClient( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg );

  // Messages
  void onMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary );
  void onJsonRPCResponse( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json response );
  void onJsonRPCRequest( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json request );
  // Others
  void onOpen( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& uri, ix::WebSocketHttpHeaders& headers, const std::string& protocol );
  void onClose( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::uint16_t code, const std::string& reason, bool remote );
  void onReject( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::uint16_t code, const std::string& reason, bool remote );
  void onPing( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary );
  void onPong( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary );

  std::mutex                                                                m_map_mutex;
  std::unordered_map<jsonrpc::id_t, std::shared_ptr<yaodaq::ServerRequest>> m_server_construct_response;

  // The server did its own requests
  std::mutex                                                                m_server_own_request;
  std::unordered_map<jsonrpc::id_t, std::shared_ptr<yaodaq::ServerRequest>> m_server_own_construct_response;

  //
  //void onFragment( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary );
  //void onError( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::uint32_t retries, const double wait_time, const int http_status, const std::string& reason, const bool decompressionError );

  // Send to all clients except webSocket
  void sendExcept( const std::string& str, ix::WebSocket& webSocket );
  // Send only to webSocket
  void sendTo( const std::string& str, ix::WebSocket& webSocket );
  // Send to all
  void sendToAll( const std::string& str );

  // Check if a client already have this name
  std::unordered_set<std::string> m_clients;
  std::mutex                      m_mutex;
};

}  // namespace yaodaq
