#pragma once

#include "yaodaq/Identifier.hpp"

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

  void setVerbosity( const spdlog::level::level_enum& level ) { m_logger->set_level( level ); }

  template<typename... Args> void trace( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->trace( str, std::forward<Args>( args )... );
    m_logger_websocket->trace( str, std::forward<Args>( args )... );
  }
  template<typename... Args> void debug( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->debug( str, std::forward<Args>( args )... );
    m_logger_websocket->debug( str, std::forward<Args>( args )... );
  }
  template<typename... Args> void info( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->info( str, std::forward<Args>( args )... );
    m_logger_websocket->info( str, std::forward<Args>( args )... );
  }
  template<typename... Args> void warn( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->warn( str, std::forward<Args>( args )... );
    m_logger_websocket->warn( str, std::forward<Args>( args )... );
  }
  template<typename... Args> void error( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->error( str, std::forward<Args>( args )... );
    m_logger_websocket->error( str, std::forward<Args>( args )... );
  }
  template<typename... Args> void critical( const std::string_view& str, Args&&... args ) const noexcept
  {
    m_logger->critical( str, std::forward<Args>( args )... );
    m_logger_websocket->critical( str, std::forward<Args>( args )... );
  }

protected:
  template<typename... Args> void trace_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->trace( str, std::forward<Args>( args )... ); }
  template<typename... Args> void debug_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->debug( str, std::forward<Args>( args )... ); }
  template<typename... Args> void info_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->info( str, std::forward<Args>( args )... ); }
  template<typename... Args> void warn_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->warn( str, std::forward<Args>( args )... ); }
  template<typename... Args> void error_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->error( str, std::forward<Args>( args )... ); }
  template<typename... Args> void critical_without_websocket( const std::string_view& str, Args&&... args ) const noexcept { m_logger->critical( str, std::forward<Args>( args )... ); }

private:
  std::shared_ptr<spdlog::logger> m_logger{ nullptr };
  std::shared_ptr<spdlog::logger> m_logger_websocket{ nullptr };
  mutable std::mutex              m_mutex;
};

}  // namespace yaodaq
