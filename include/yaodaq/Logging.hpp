#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/Identifier.hpp"
#include "yaodaq/Verbosity.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
#include <string_view>

namespace yaodaq
{

class Log;

class Logging
{
public:
  explicit Logging() noexcept = delete;
  explicit Logging( const Identifier& identifier );

  void add_websocket_callback( const std::function<void( const Log& msg )>& f );

  void setVerbosity( const yaodaq::verbosity::level level )
  {
    m_verbosity = level;
    // just in case spdlog change the numbers;
    switch( level )
    {
      case verbosity::level::off:
      {
        if( m_logger ) m_logger->set_level( spdlog::level::level_enum::off );
        break;
      }
      case verbosity::level::critical:
      {
        if( m_logger ) m_logger->set_level( spdlog::level::level_enum::critical );
        break;
      }
      case verbosity::level::error:
      {
        if( m_logger ) m_logger->set_level( spdlog::level::level_enum::err );
        break;
      }
      case verbosity::level::warning:
      {
        if( m_logger ) m_logger->set_level( spdlog::level::level_enum::warn );
        break;
      }
      case verbosity::level::info:
      {
        if( m_logger ) m_logger->set_level( spdlog::level::level_enum::info );
        break;
      }
      case verbosity::level::debug:
      {
        if( m_logger ) m_logger->set_level( spdlog::level::level_enum::debug );
        break;
      }
      case verbosity::level::trace:
      {
        if( m_logger ) m_logger->set_level( spdlog::level::level_enum::trace );
        break;
      }
    }
  }

  yaodaq::verbosity::level getVerbosity() const noexcept { return m_verbosity; }

  template<typename... Args> void trace( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->trace( fmt::runtime( str ), std::forward<Args>( args )... );
    if( m_logger_websocket ) m_logger_websocket->trace( fmt::runtime( str ), std::forward<Args>( args )... );
  }
  template<typename... Args> void debug( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->debug( fmt::runtime( str ), std::forward<Args>( args )... );
    if( m_logger_websocket ) m_logger_websocket->debug( fmt::runtime( str ), std::forward<Args>( args )... );
  }
  template<typename... Args> void info( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->info( fmt::runtime( str ), std::forward<Args>( args )... );
    if( m_logger_websocket ) m_logger_websocket->info( fmt::runtime( str ), std::forward<Args>( args )... );
  }
  template<typename... Args> void warn( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->warn( fmt::runtime( str ), std::forward<Args>( args )... );
    if( m_logger_websocket ) m_logger_websocket->warn( fmt::runtime( str ), std::forward<Args>( args )... );
  }
  template<typename... Args> void error( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->error( fmt::runtime( str ), std::forward<Args>( args )... );
    if( m_logger_websocket ) m_logger_websocket->error( fmt::runtime( str ), std::forward<Args>( args )... );
  }
  template<typename... Args> void critical( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->critical( fmt::runtime( str ), std::forward<Args>( args )... );
    if( m_logger_websocket ) m_logger_websocket->critical( fmt::runtime( str ), std::forward<Args>( args )... );
  }

protected:
  friend class Client;
  template<typename... Args> void trace_without_websocket( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->trace( fmt::runtime( str ), std::forward<Args>( args )... );
  }
  template<typename... Args> void debug_without_websocket( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->debug( fmt::runtime( str ), std::forward<Args>( args )... );
  }
  template<typename... Args> void info_without_websocket( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->info( fmt::runtime( str ), std::forward<Args>( args )... );
  }
  template<typename... Args> void warn_without_websocket( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->warn( fmt::runtime( str ), std::forward<Args>( args )... );
  }
  template<typename... Args> void error_without_websocket( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->error( fmt::runtime( str ), std::forward<Args>( args )... );
  }
  template<typename... Args> void critical_without_websocket( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_logger ) m_logger->critical( fmt::runtime( str ), std::forward<Args>( args )... );
  }

private:
  std::shared_ptr<spdlog::logger> m_logger{ nullptr };
  std::shared_ptr<spdlog::logger> m_logger_websocket{ nullptr };
  yaodaq::verbosity::level        m_verbosity{ yaodaq::verbosity::level::info };
  mutable std::mutex              m_mutex;
};

class LoggableClient
{
protected:
  void setLogger( std::shared_ptr<Logging> log ) { m_log = std::move( log ); }

public:
  explicit LoggableClient() = default;
  template<typename... Args> void trace( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->trace( str, std::forward<Args>( args )... );
  }
  template<typename... Args> void debug( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->debug( str, std::forward<Args>( args )... );
  }
  template<typename... Args> void info( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->info( str, std::forward<Args>( args )... );
  }
  template<typename... Args> void warn( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->warn( str, std::forward<Args>( args )... );
  }
  template<typename... Args> void error( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->error( str, std::forward<Args>( args )... );
  }
  template<typename... Args> void critical( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->critical( str, std::forward<Args>( args )... );
  }

private:
  std::shared_ptr<Logging> m_log;
};

class Loggable
{
public:
  explicit Loggable( const Identifier& id ) : m_id( id.id() ) {}
  void setLogger( std::shared_ptr<Logging> log ) { m_log = std::move( log ); }

protected:
  template<typename... Args> void trace( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->info( "[{}] {}", m_id, fmt::format( fmt::runtime( str ), std::forward<Args>( args )... ) );
  }
  template<typename... Args> void debug( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->debug( "[{}] {}", m_id, fmt::format( fmt::runtime( str ), std::forward<Args>( args )... ) );
  }
  template<typename... Args> void info( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->info( "[{}] {}", m_id, fmt::format( fmt::runtime( str ), std::forward<Args>( args )... ) );
  }
  template<typename... Args> void warn( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->warn( "[{}] {}", m_id, fmt::format( fmt::runtime( str ), std::forward<Args>( args )... ) );
  }
  template<typename... Args> void error( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->error( "[{}] {}", m_id, fmt::format( fmt::runtime( str ), std::forward<Args>( args )... ) );
  }
  template<typename... Args> void critical( const std::string_view& str, Args&&... args ) const noexcept
  {
    if( m_log ) m_log->critical( "[{}] {}", m_id, fmt::format( fmt::runtime( str ), std::forward<Args>( args )... ) );
  }

private:
  std::shared_ptr<Logging> m_log;
  std::string              m_id;
};

}  // namespace yaodaq
