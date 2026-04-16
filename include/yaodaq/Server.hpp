#pragma once
#include "yaodaq/Defaults.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Identifier.hpp"
#include "yaodaq/JsonRPCAsker.hpp"
#include "yaodaq/JsonRPCResponder.hpp"
#include "yaodaq/Logging.hpp"
#include "yaodaq/ThreadPool.hpp"

#include <cstddef>
#include <cstdint>
#include <ixwebsocket/IXWebSocketServer.h>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace yaodaq
{

class ServerConfig
{
public:
  YAODAQ_API explicit ServerConfig() noexcept = default;
  YAODAQ_API ServerConfig&       operator()() noexcept { return *this; }
  YAODAQ_API const ServerConfig& operator()() const noexcept { return *this; }
  // Fluent setters
  YAODAQ_API ServerConfig&       setHost( const std::string_view h )
  {
    m_host = std::string( h );
    return *this;
  }
  YAODAQ_API ServerConfig& setPort( std::uint16_t p )
  {
    m_port = p;
    return *this;
  }
  YAODAQ_API ServerConfig& setMaxConnections( std::size_t p )
  {
    m_maxConnections = p;
    return *this;
  }
  YAODAQ_API ServerConfig& setHandshakeTimeoutSecs( std::size_t p )
  {
    m_handshakeTimeoutSecs = p;
    return *this;
  }
  YAODAQ_API ServerConfig& setPingIntervalSeconds( std::size_t p )
  {
    m_pingIntervalSeconds = p;
    return *this;
  }
  YAODAQ_API ServerConfig& setBacklog( std::size_t p )
  {
    m_backlog = p;
    return *this;
  }
  YAODAQ_API ServerConfig& setAdressFamily( int p )
  {
    m_addressFamily = p;
    return *this;
  }
  YAODAQ_API ServerConfig& setPath( const std::string_view p )
  {
    m_path = std::string( p );
    return *this;
  }
  YAODAQ_API ServerConfig& enableTLS( bool t = true )
  {
    m_tls = t;
    return *this;
  }
  YAODAQ_API ServerConfig& certFile( std::string& file )
  {
    m_certFile = file;
    return *this;
  }
  YAODAQ_API ServerConfig& keyFile( std::string& file )
  {
    m_keyFile = file;
    return *this;
  }
  YAODAQ_API ServerConfig& caFile( std::string& file )
  {
    m_caFile = file;
    return *this;
  }
  YAODAQ_API const std::string_view getHost() const noexcept { return m_host; }
  YAODAQ_API std::uint16_t getPort() const noexcept { return m_port; }
  YAODAQ_API std::size_t getMaxConnections() const noexcept { return m_maxConnections; }
  YAODAQ_API const std::string_view getPath() const noexcept { return m_path; }
  YAODAQ_API std::size_t getHandshakeTimeoutSecs() const noexcept { return m_handshakeTimeoutSecs; }
  YAODAQ_API int         getPingIntervalSeconds() const noexcept { return m_pingIntervalSeconds; }
  YAODAQ_API std::size_t getBacklog() const noexcept { return m_backlog; }
  YAODAQ_API int         getAddressFamily() const noexcept { return m_addressFamily; }
  YAODAQ_API bool        isTLS() const noexcept { return m_tls; }
  YAODAQ_API const std::string& certFile() const noexcept { return m_certFile; }
  YAODAQ_API const std::string& keyFile() const noexcept { return m_keyFile; }
  YAODAQ_API const std::string& caFile() const noexcept { return m_caFile; }

private:
  std::string   m_host{ defaults::host };
  std::uint16_t m_port{ defaults::port };
  std::size_t   m_maxConnections{ defaults::max_connections };
  std::string   m_path{ defaults::path };
  std::size_t   m_handshakeTimeoutSecs{ defaults::handshake_timeout };
  int           m_pingIntervalSeconds{ defaults::ping_interval_seconds };
  std::size_t   m_backlog{ defaults::backlog };
  int           m_addressFamily{ defaults::ip6 ? AF_INET6 : AF_INET };
  bool          m_tls{ false };
  std::string   m_certFile;
  std::string   m_keyFile;
  std::string   m_caFile{ "SYSTEM" };
};

struct ServerRequest
{
  std::mutex                                             mtx;  // protect shared state
  std::condition_variable                                cv;
  std::unordered_map<yaodaq::Identifier, nlohmann::json> responses;
  std::size_t                                            expected_responses{ 0 };
  std::size_t                                            received_responses{ 0 };
};

class Server : public ix::WebSocketServer, public JsonRPCResponder, public Logging, public JsonRPCAsker
{
public:
  YAODAQ_API ~Server() noexcept;
  YAODAQ_API Server( const ServerConfig& cfg, const std::string_view name, const std::string_view type = "yaodaq" );

  YAODAQ_API const Identifier& identifier() const noexcept { return m_identifier; }

  YAODAQ_API void start();

  YAODAQ_API bool loop();

  YAODAQ_API void rejectBrowsers() noexcept { m_rejectBrowser = true; }

  YAODAQ_API std::size_t getNumberOfClients() noexcept;

private:
  YAODAQ_API explicit Server() noexcept = delete;
  Identifier m_identifier;
  ThreadPool m_threadPool;
  bool       m_ssl{ false };
  bool       m_rejectBrowser{ false };  //< Reject the Browsers

  virtual void Send( const std::string_view request ) override final;
  /* Check if client has all to be an yaodaq one and if he has the authorizations */
  void         checkClient( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg );

  // Messages
  void onMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary );
  void onJsonRPCResponse( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json response );
  void onJsonRPCRequest( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json request );
  void onLog( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json request );
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
  // Send to loggers
  void sendToLoggers( const std::string& str );

  // Check if a client already have this name
  std::unordered_set<std::string>                                                         m_clients;
  std::unordered_map<Component::Role, std::vector<std::reference_wrapper<ix::WebSocket>>> m_clientss;
  std::mutex                                                                              m_mutex;
};

}  // namespace yaodaq
