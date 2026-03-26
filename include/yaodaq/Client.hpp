#pragma once
#include "yaodaq/Defaults.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Identifier.hpp"
#include "yaodaq/JsonRPCHandler.hpp"
#include "yaodaq/Log.hpp"

#include <cstdint>
#include <ixwebsocket/IXWebSocket.h>
#include <string>
#include <string_view>

namespace yaodaq
{

class YAODAQ_API Client : public JsonRPCHandler, public ix::WebSocket, public Log  //TODO : Why YAODAQ_API is needed here ?
{
public:
  YAODAQ_API virtual ~Client() noexcept;
  YAODAQ_API explicit Client( const Identifier& identifier, const std::string_view host = defaults::host, const std::uint16_t port = defaults::port, const std::string_view path = defaults::path );

  YAODAQ_API const Identifier& identifier() const noexcept { return m_identifier; }

  YAODAQ_API void setURL( const std::string_view host = "127.0.0.1", const std::uint16_t port = 8888, const std::string_view path = "/" )
  {
    std::string cleanHost( host );
    if( !cleanHost.empty() && cleanHost.back() == '/' ) cleanHost.pop_back();

    std::string url = cleanHost + ':' + std::to_string( port );

    // Append path carefully
    std::string cleanPath( path );
    if( !cleanPath.empty() )
    {
      if( cleanPath.front() == '/' ) url += cleanPath;
      else
        url += '/' + cleanPath;
    }
    else
      url += '/';
    m_url = std::move( url );
  }

protected:
  YAODAQ_API void start()
  {
    m_url = m_tls ? "wss://" : "ws://" + m_url;
    ix::WebSocket::setUrl( m_url );  // Read twice folks !!!
    ix::WebSocket::start();
    logger()->info( "trying to connect to {}", m_url );
  }

  /*YAODAQ_API void setTLS( const std::string& certFile, const std::string& keyFile, const std::string& caFile = "SYSTEM" )
  {
    m_tls = true;
    m_tlsOptions.certFile = certFile;
    m_tlsOptions.keyFile  = keyFile;
    m_tlsOptions.caFile   = caFile;
    m_tlsOptions.tls      = true;
    setTLSOptions( m_tlsOptions );
  }*/
private:
  YAODAQ_API explicit Client() noexcept = delete;
  const Identifier m_identifier;
  //ix::SocketTLSOptions m_tlsOptions;

  virtual void    Send( const std::string_view request ) override final;
  // Messages
  YAODAQ_API void onMessage( const std::string& str, const std::size_t size, const bool binary );
  YAODAQ_API void onJsonRPC( const nlohmann::json& json );
  // Others
  YAODAQ_API void onOpen( const std::string& uri, const ix::WebSocketHttpHeaders& headers, const std::string& protocol );
  YAODAQ_API void onClose( const std::uint16_t code, const std::string& reason, bool remote );
  YAODAQ_API void onReject( const std::uint16_t code, const std::string& reason, bool remote );  // Has been closed by server because of some criteria

  /*YAODAQ_API void onFragment( const std::string& str, const std::size_t size, const bool binary );


  YAODAQ_API void onError( const std::uint32_t retries, const double wait_time, const int http_status, const std::string& reason, const bool decompressionError );*/
  YAODAQ_API void onPing( const std::string& str, const std::size_t size, const bool binary );
  YAODAQ_API void onPong( const std::string& str, const std::size_t size, const bool binary );

  /*YAODAQ_API void onText( const std::string& text );
  YAODAQ_API void onJson( const nlohmann::json& json );*/

  std::string m_url;
  bool        m_tls{ false };
};

}  // namespace yaodaq
