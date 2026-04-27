#pragma once

#include "yaodaq/Identifier.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>

namespace yaodaq
{

class Log;

class Logging
{
public:
  explicit Logging() noexcept = delete;
  explicit Logging( const Identifier& identifier );

  const std::shared_ptr<spdlog::logger> logger() const noexcept
  {
    m_ws_enabled = true;
    return m_logger;
  }
  void add_websocket_callback( const std::function<void( const Log& msg )>& f );

protected:
  std::shared_ptr<spdlog::logger> logger_without_websocket() const noexcept
  {
    m_ws_enabled = false;
    return m_logger;
  }

private:
  const std::shared_ptr<spdlog::logger> m_logger{ nullptr };
  std::mutex                            m_mutex;
  inline static thread_local bool       m_ws_enabled{ true };
};

}  // namespace yaodaq
