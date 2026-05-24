#pragma once
#include "yaodaq/Connector.hpp"
#include "yaodaq/Defaults.hpp"
#include "yaodaq/Exception.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Verbosity.hpp"

#include <cstdint>
#include <ixwebsocket/IXUrlParser.h>
#include <memory>
#include <string>
#include <string_view>
#include <sys/socket.h>

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
  YAODAQ_API ServerConfig& verbosity( const yaodaq::verbosity::level verbosity )
  {
    m_verbosity = verbosity;
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
  YAODAQ_API const yaodaq::verbosity::level getVerbosity() const noexcept { return m_verbosity; }

private:
  std::string              m_host{ defaults::host };
  std::uint16_t            m_port{ defaults::port };
  std::size_t              m_maxConnections{ defaults::max_connections };
  std::string              m_path{ defaults::path };
  std::size_t              m_handshakeTimeoutSecs{ defaults::handshake_timeout };
  int                      m_pingIntervalSeconds{ defaults::ping_interval_seconds };
  std::size_t              m_backlog{ defaults::backlog };
  int                      m_addressFamily{ defaults::ip6 ? AF_INET6 : AF_INET };
  bool                     m_tls{ false };
  std::string              m_certFile;
  std::string              m_keyFile;
  std::string              m_caFile{ "SYSTEM" };
  yaodaq::verbosity::level m_verbosity{ yaodaq::verbosity::level::info };
};

class Config
{
public:
  YAODAQ_API explicit Config() noexcept = default;
  YAODAQ_API Config&       operator()() noexcept { return *this; }
  YAODAQ_API const Config& operator()() const noexcept { return *this; }
  // Fluent setters
  YAODAQ_API Config&       setHost( const std::string_view h )
  {
    m_host = std::string( h );
    return *this;
  }
  YAODAQ_API Config& setPort( std::uint16_t p )
  {
    m_port = p;
    return *this;
  }
  YAODAQ_API Config& setPath( const std::string_view p )
  {
    m_path = std::string( p );
    return *this;
  }
  YAODAQ_API Config& enableTLS( bool t = true )
  {
    m_tls = t;
    return *this;
  }
  YAODAQ_API Config& certFile( std::string& file )
  {
    m_certFile = file;
    return *this;
  }
  YAODAQ_API Config& keyFile( std::string& file )
  {
    m_keyFile = file;
    return *this;
  }
  YAODAQ_API Config& caFile( std::string& file )
  {
    m_caFile = file;
    return *this;
  }
  YAODAQ_API Config& verbosity( const yaodaq::verbosity::level verbosity )
  {
    m_verbosity = verbosity;
    return *this;
  }

  YAODAQ_API const std::string_view getHost() const noexcept { return m_host; }
  YAODAQ_API std::uint16_t getPort() const noexcept { return m_port; }
  YAODAQ_API const std::string_view getPath() const noexcept { return m_path; }
  YAODAQ_API const yaodaq::verbosity::level getVerbosity() const noexcept { return m_verbosity; }
  YAODAQ_API bool                           isTLS() const noexcept { return m_tls; }
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

    std::string protocol;
    std::string host;
    std::string path;
    std::string query;
    int         port;
    bool        ret = ix::UrlParser::parse( url, protocol, host, path, query, port );
    if( ret ) return url;
    else
      throw yaodaq::Exception( "url not valid" );
  }
  YAODAQ_API const std::string& certFile() const noexcept { return m_certFile; }
  YAODAQ_API const std::string& keyFile() const noexcept { return m_keyFile; }
  YAODAQ_API const std::string& caFile() const noexcept { return m_caFile; }

private:
  std::string              m_host{ defaults::host };
  std::uint16_t            m_port{ defaults::port };
  std::string              m_path{ defaults::path };
  std::string              m_certFile;
  std::string              m_keyFile;
  std::string              m_caFile{ "SYSTEM" };
  bool                     m_tls{ false };
  yaodaq::verbosity::level m_verbosity{ yaodaq::verbosity::level::info };
};

class BoardConfig : public Config
{
public:
  explicit BoardConfig( std::unique_ptr<Connector> connector ) : m_Connector( std::move( connector ) ) {}
  std::unique_ptr<Connector> getConnector() const noexcept { return std::move( m_Connector ); }

private:
  mutable std::unique_ptr<Connector> m_Connector{ nullptr };
};

}  // namespace yaodaq
