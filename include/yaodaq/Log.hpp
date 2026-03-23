#pragma once
#include "yaodaq/Identifier.hpp"

#include <memory>
#include <mutex>
#include <spdlog/logger.h>

namespace yaodaq
{

class Log
{
public:
  explicit Log() = delete;
  explicit Log( const Identifier& identifier );
  std::shared_ptr<spdlog::logger> logger() const { return m_logger; }

private:
  std::shared_ptr<spdlog::logger> m_logger{ nullptr };
  std::mutex                      m_mutex;
};

}  // namespace yaodaq
