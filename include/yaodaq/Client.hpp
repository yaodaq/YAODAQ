#pragma once
#include "yaodaq/Cleaner.hpp"
#include "yaodaq/Config.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Identifier.hpp"
#include "yaodaq/JSONCodec.hpp"
#include "yaodaq/JsonRPCResponder.hpp"
#include "yaodaq/Logging.hpp"
#include "yaodaq/Message.hpp"

#include <cstdint>
#include <functional>
#include <ixwebsocket/IXWebSocket.h>
#include <new>
#include <span>
#include <string>
#include <string_view>

namespace yaodaq
{

class YAODAQ_API Client : public JsonRPCResponder, public LoggableBase  //TODO : Why YAODAQ_API is needed here ?
{
public:
  YAODAQ_API ~Client() noexcept override;
  Client( const Client& )            = delete;
  Client& operator=( const Client& ) = delete;
  Client( Client&& )                 = delete;
  Client& operator=( Client&& )      = delete;
  YAODAQ_API explicit Client( const Identifier& id, const Config& client_config );

  YAODAQ_API const Identifier& identifier() const noexcept { return m_identifier; }
  YAODAQ_API virtual bool      cleanup()
  {
    debug( "Client cleanup" );
    close();
    return true;
  }
  enum class send_as : std::uint8_t
  {
    utf8   = 1,
    text   = 2,
    binary = 3,
  };
  YAODAQ_API void send( std::span<const std::byte> raw ) noexcept
  {
    const std::string_view sv( std::bit_cast<const char*>( raw.data() ), raw.size() );
    send( sv, send_as::binary );
  }
  YAODAQ_API void send( const std::string_view, const send_as as = send_as::utf8 ) noexcept;

protected:
  YAODAQ_API void          send( const Message& message ) noexcept;
  std::shared_ptr<Logging> get_logger() const noexcept { return m_log; }
  YAODAQ_API ix::WebSocket& getWebsocketClient() noexcept { return m_client; }
  YAODAQ_API void           start()
  {
    m_client.start();
    info( "trying to connect to {}", m_client.getUrl() );
  }
  YAODAQ_API void close()
  {
    info( "Closing {}", m_client.getUrl() );
    m_client.close();
  }
  YAODAQ_API virtual void onResponse( const std::string& ) { /* Standard client don't receive response !!  Only Controller has the right to ask */ }

private:
  YAODAQ_API explicit Client() noexcept = delete;
  Identifier               m_identifier;
  ix::WebSocket            m_client;
  JSONCodec                m_json_codec;
  std::shared_ptr<Logging> m_log;
  YAODAQ_INTERNAL void     handleMessage( const ix::WebSocketMessagePtr& msg ) noexcept;

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
  YAODAQ_INTERNAL void onLog( const std::unique_ptr<Log> log );
};

}  // namespace yaodaq
