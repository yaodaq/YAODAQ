#pragma once
#include "yaodaq/ClientRegistry.hpp"
#include "yaodaq/Config.hpp"
#include "yaodaq/Defaults.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/ICodec.hpp"
#include "yaodaq/Identifier.hpp"
#include "yaodaq/JsonRPCAsker.hpp"
#include "yaodaq/JsonRPCResponder.hpp"
#include "yaodaq/Logging.hpp"
#include "yaodaq/Message.hpp"
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

struct ServerRequest
{
  std::mutex                                          mtx;  // protect shared state
  std::condition_variable                             cv;
  std::unordered_map<yaodaq::Identifier, std::string> responses;
  std::size_t                                         expected_responses{ 0 };
  std::size_t                                         received_responses{ 0 };
};

class Server : public JsonRPCResponder, public Logging, public JsonRPCAsker
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
  YAODAQ_INTERNAL explicit Server() noexcept = delete;
  ix::WebSocketServer  m_server;
  Identifier           m_identifier;
  ThreadPool           m_threadPool;
  YAODAQ_INTERNAL void handleMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg ) noexcept;
  bool                 m_rejectBrowser{ false };  //< Reject the Browsers

  virtual void         Send( const std::string_view request ) override final;
  /* Check if client has all to be an yaodaq one and if he has the authorizations */
  YAODAQ_INTERNAL void checkClient( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg );

  // Messages
  YAODAQ_INTERNAL void onMessage( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const std::string& str, const std::size_t size, const bool binary );
  YAODAQ_INTERNAL void onJsonRPCResponse( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json response );
  YAODAQ_INTERNAL void onJsonRPCRequest( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json request );
  YAODAQ_INTERNAL void onLog( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, nlohmann::json request );
  // Others
  YAODAQ_INTERNAL void onOpen( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Open& open );
  YAODAQ_INTERNAL void onClose( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Close& close );
  YAODAQ_INTERNAL void onReject( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Reject& reject );
  YAODAQ_INTERNAL void onPing( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Ping& ping );
  YAODAQ_INTERNAL void onPong( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Pong& pong );
  YAODAQ_INTERNAL void onError( std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const Error& error );

  std::mutex                                                                m_map_mutex;
  std::unordered_map<jsonrpc::id_t, std::shared_ptr<yaodaq::ServerRequest>> m_server_construct_response;

  // The server did its own requests
  std::mutex                                                                m_server_own_request;
  std::unordered_map<jsonrpc::id_t, std::shared_ptr<yaodaq::ServerRequest>> m_server_own_construct_response;

  // Send to all clients except webSocket
  YAODAQ_INTERNAL void sendExcept( const std::string_view& str, ix::WebSocket& webSocket );
  // Send only to webSocket
  YAODAQ_INTERNAL void sendTo( const std::string_view& str, ix::WebSocket& webSocket );
  // Send to all
  YAODAQ_INTERNAL void sendToAll( const std::string_view& str ) noexcept;
  // Send to loggers
  YAODAQ_INTERNAL void sendToLoggers( const std::string_view& str );

  // Check if a client already have this name
  //std::unordered_map<Component::Role, std::unordered_map<std::string, std::reference_wrapper<ix::WebSocket>>> m_clients;
  std::mutex     m_mutex;
  ClientRegistry m_registry;
};

}  // namespace yaodaq
