#pragma once

#include "yaodaq/Identifier.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>
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

  template<typename... Args> void trace( const std::string_view& str, Args&&... args ) const noexcept { m_logger->trace( str, std::forward<Args>( args )... ); }
  template<typename... Args> void debug( const std::string_view& str, Args&&... args ) const noexcept { m_logger->debug( str, std::forward<Args>( args )... ); }
  template<typename... Args> void info( const std::string_view& str, Args&&... args ) const noexcept { m_logger->info( str, std::forward<Args>( args )... ); }
  template<typename... Args> void warn( const std::string_view& str, Args&&... args ) const noexcept { m_logger->warn( str, std::forward<Args>( args )... ); }
  template<typename... Args> void error( const std::string_view& str, Args&&... args ) const noexcept { m_logger->error( str, std::forward<Args>( args )... ); }
  template<typename... Args> void critical( const std::string_view& str, Args&&... args ) const noexcept { m_logger->critical( str, std::forward<Args>( args )... ); }

protected:
  std::shared_ptr<spdlog::logger> logger_without_websocket() const noexcept
  {
    m_ws_enabled = false;
    return m_logger;
  }

private:
  const std::shared_ptr<spdlog::logger> m_logger{ nullptr };
  mutable std::mutex                    m_mutex;
  inline static thread_local bool       m_ws_enabled{ true };
};

}  // namespace yaodaq
