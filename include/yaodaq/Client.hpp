#pragma once

#include "yaodaq/Config.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Identifier.hpp"
#include "yaodaq/JsonRPCResponder.hpp"
#include "yaodaq/Logging.hpp"
#include "yaodaq/Message.hpp"

#include <cstdint>
#include <functional>
#include <ixwebsocket/IXWebSocket.h>
#include <new>
#include <string>
#include <string_view>

namespace yaodaq
{

class YAODAQ_API Client : public JsonRPCResponder, public Logging  //TODO : Why YAODAQ_API is needed here ?
{
public:
  YAODAQ_API virtual ~Client() noexcept;
  YAODAQ_API explicit Client( const Identifier& id, const Config& client_config );

  YAODAQ_API const Identifier& identifier() const noexcept { return m_identifier; }

protected:
  ix::WebSocket&  getWebsocketClient() noexcept { return m_client; }
  YAODAQ_API void start()
  {
    m_client.start();
    info( "trying to connect to {}", m_client.getUrl() );
  }
  YAODAQ_API void close()
  {
    info( "Closing {}", m_client.getUrl() );
    m_client.close();
  }
  virtual void onResponse( const std::string& ) {}

private:
  const Identifier m_identifier;
  ix::WebSocket    m_client;
  explicit Client() noexcept = delete;
  YAODAQ_INTERNAL void handleMessage( const ix::WebSocketMessagePtr& msg ) noexcept;

  // Messages
  YAODAQ_INTERNAL void onMessage( const std::string& str, const std::size_t size, const bool binary );
  // Others
  YAODAQ_INTERNAL void onOpen( const Open& open );
  YAODAQ_INTERNAL void onClose( const Close& close );
  YAODAQ_INTERNAL void onReject( const Reject& reject );  // Has been closed by server because of some criteria
  YAODAQ_INTERNAL void onError( const Error& error );
  /*YAODAQ_API void onFragment( const std::string& str, const std::size_t size, const bool binary );*/

  YAODAQ_INTERNAL void onPing( const Ping& ping );
  YAODAQ_INTERNAL void onPong( const Pong& pong );
  YAODAQ_INTERNAL void onLog( const nlohmann::json& json );

  enum class send_as : unsigned char
  {
    utf8   = 1,
    binary = 2,
  };
  YAODAQ_INTERNAL void send( const Message& message, const send_as as = send_as::utf8 ) noexcept;
};

}  // namespace yaodaq
