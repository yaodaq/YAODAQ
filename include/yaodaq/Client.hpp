#pragma once
#include "yaodaq/Defaults.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Identifier.hpp"
#include "yaodaq/JsonRPCResponder.hpp"
#include "yaodaq/Logging.hpp"

#include <cstdint>
#include <functional>
#include <ixwebsocket/IXWebSocket.h>
#include <new>
#include <string>
#include <string_view>

namespace yaodaq
{

class ClientConfig
{
public:
  YAODAQ_API explicit ClientConfig() noexcept = default;
  YAODAQ_API ClientConfig&       operator()() noexcept { return *this; }
  YAODAQ_API const ClientConfig& operator()() const noexcept { return *this; }
  // Fluent setters
  YAODAQ_API ClientConfig&       setHost( const std::string_view h )
  {
    m_host = std::string( h );
    return *this;
  }
  YAODAQ_API ClientConfig& setPort( uint16_t p )
  {
    m_port = p;
    return *this;
  }
  YAODAQ_API ClientConfig& setPath( const std::string_view p )
  {
    m_path = std::string( p );
    return *this;
  }
  YAODAQ_API ClientConfig& enableTLS( bool t = true )
  {
    m_tls = t;
    return *this;
  }
  YAODAQ_API ClientConfig& certFile( std::string& file )
  {
    m_certFile = file;
    return *this;
  }
  YAODAQ_API ClientConfig& keyFile( std::string& file )
  {
    m_keyFile = file;
    return *this;
  }
  YAODAQ_API ClientConfig& caFile( std::string& file )
  {
    m_caFile = file;
    return *this;
  }

  YAODAQ_API const std::string_view getHost() const noexcept { return m_host; }
  YAODAQ_API std::uint16_t getPort() const noexcept { return m_port; }
  YAODAQ_API const std::string_view getPath() const noexcept { return m_path; }
  YAODAQ_API bool                   isTLS() const noexcept { return m_tls; }
  YAODAQ_API std::string url() const
  {
    std::string cleanHost( m_host );
    if( !cleanHost.empty() && cleanHost.back() == '/' ) cleanHost.pop_back();
    std::string url = cleanHost + ':' + std::to_string( m_port );
    std::string cleanPath( m_path );
    if( !cleanPath.empty() )
    {
      if( cleanPath.front() == '/' ) url += cleanPath;
      else
        url += '/' + cleanPath;
    }
    else
      url += '/';
    url = m_tls ? "wss://" : "ws://" + url;
    return url;
  }
  YAODAQ_API const std::string& certFile() const noexcept { return m_certFile; }
  YAODAQ_API const std::string& keyFile() const noexcept { return m_keyFile; }
  YAODAQ_API const std::string& caFile() const noexcept { return m_caFile; }

private:
  std::string   m_host{ defaults::host };
  std::uint16_t m_port{ defaults::port };
  std::string   m_path{ defaults::path };
  std::string   m_certFile;
  std::string   m_keyFile;
  std::string   m_caFile{ "SYSTEM" };
  bool          m_tls{ false };
};

class YAODAQ_API Client : public JsonRPCResponder, public ix::WebSocket, public Logging  //TODO : Why YAODAQ_API is needed here ?
{
public:
  YAODAQ_API virtual ~Client() noexcept;
  YAODAQ_API explicit Client( const Identifier& id, const ClientConfig& client_config );

  YAODAQ_API const Identifier& identifier() const noexcept { return m_identifier; }

protected:
  YAODAQ_API void start()
  {
    ix::WebSocket::start();
    logger()->info( "trying to connect to {}", m_url );
  }
  virtual void     onResponse( const std::string& ) {}
  const Identifier m_identifier;

private:
  std::string m_url;
  YAODAQ_API explicit Client() noexcept = delete;

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
  YAODAQ_API void onLog( const nlohmann::json& json );

  /*YAODAQ_API void onText( const std::string& text );
  YAODAQ_API void onJson( const nlohmann::json& json );*/
};

}  // namespace yaodaq
