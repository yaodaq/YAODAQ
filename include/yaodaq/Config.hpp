#pragma once
#include "yaodaq/Defaults.hpp"
#include "yaodaq/Export.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace yaodaq
{

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

}  // namespace yaodaq
