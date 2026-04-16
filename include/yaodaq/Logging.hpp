#pragma once
#include "yaodaq/Identifier.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <spdlog/logger.h>

namespace yaodaq
{

class Logging
{
public:
  explicit Logging() = delete;
  explicit Logging( const Identifier& identifier );
  std::shared_ptr<spdlog::logger> logger() const { return m_logger; }
  void                            add_callback( std::function<void( const spdlog::details::log_msg& msg )>& f );

private:
  std::shared_ptr<spdlog::logger> m_logger{ nullptr };
  std::mutex                      m_mutex;
};

}  // namespace yaodaq
