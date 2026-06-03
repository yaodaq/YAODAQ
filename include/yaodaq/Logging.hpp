#pragma once

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

  void setVerbosity( const yaodaq::verbosity::level& level )
  {
    m_verbosity = level;
    // just in case spdlog change the numbers;
    switch( level )
    {
      case verbosity::level::off:
      {
        m_logger->set_level( spdlog::level::level_enum::off );
        break;
      }
      case verbosity::level::critical:
      {
        m_logger->set_level( spdlog::level::level_enum::critical );
        break;
      }
      case verbosity::level::error:
      {
        m_logger->set_level( spdlog::level::level_enum::err );
        break;
      }
      case verbosity::level::warning:
      {
        m_logger->set_level( spdlog::level::level_enum::warn );
        break;
      }
      case verbosity::level::info:
      {
        m_logger->set_level( spdlog::level::level_enum::info );
        break;
      }
      case verbosity::level::debug:
      {
        m_logger->set_level( spdlog::level::level_enum::debug );
        break;
      }
      case verbosity::level::trace:
      {
        m_logger->set_level( spdlog::level::level_enum::trace );
        break;
      }
    }
  }

  yaodaq::verbosity::level getVerbosity() const noexcept { return m_verbosity; }

  template<typename... Args> void trace( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->trace( fmt::runtime(str), std::forward<Args>( args )... );
    m_logger_websocket->trace( fmt::runtime(str), std::forward<Args>( args )... );
  }
  template<typename... Args> void debug( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->debug( fmt::runtime(str), std::forward<Args>( args )... );
    m_logger_websocket->debug( fmt::runtime(str), std::forward<Args>( args )... );
  }
  template<typename... Args> void info( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->info( fmt::runtime(str), std::forward<Args>( args )... );
    m_logger_websocket->info( fmt::runtime(str), std::forward<Args>( args )... );
  }
  template<typename... Args> void warn( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->warn( fmt::runtime(str), std::forward<Args>( args )... );
    m_logger_websocket->warn( fmt::runtime(str), std::forward<Args>( args )... );
  }
  template<typename... Args> void error( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->error( fmt::runtime(str), std::forward<Args>( args )... );
    m_logger_websocket->error( fmt::runtime(str), std::forward<Args>( args )... );
  }
  template<typename... Args> void critical( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->critical( fmt::runtime(str), std::forward<Args>( args )... );
    m_logger_websocket->critical( fmt::runtime(str), std::forward<Args>( args )... );
  }

protected:
  template<typename... Args> void trace_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->trace( fmt::runtime(str), std::forward<Args>( args )... ); }
  template<typename... Args> void debug_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->debug( fmt::runtime(str), std::forward<Args>( args )... ); }
  template<typename... Args> void info_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->info( fmt::runtime(str), std::forward<Args>( args )... ); }
  template<typename... Args> void warn_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->warn( fmt::runtime(str), std::forward<Args>( args )... ); }
  template<typename... Args> void error_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->error( fmt::runtime(str), std::forward<Args>( args )... ); }
  template<typename... Args> void critical_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->critical( fmt::runtime(str), std::forward<Args>( args )... ); }

private:
  std::shared_ptr<spdlog::logger> m_logger{ nullptr };
  std::shared_ptr<spdlog::logger> m_logger_websocket{ nullptr };
  yaodaq::verbosity::level        m_verbosity{ yaodaq::verbosity::level::info };
  mutable std::mutex              m_mutex;
};

}  // namespace yaodaq
