#pragma once
#include "yaodaq/Export.hpp"

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

namespace yaodaq
{

class YAODAQ_API verbosity
{
public:
  enum class level : unsigned char
  {
    trace,
    debug,
    info,
    warning,
    error,
    critical,
    off,
  };
  inline static const std::array<std::string_view, 7>                           list{ "trace", "debug", "info", "warning", "error", "critical", "off" };
  inline static const std::unordered_map<std::string, yaodaq::verbosity::level> map{
    { "trace", yaodaq::verbosity::level::trace }, { "debug", yaodaq::verbosity::level::debug },       { "info", yaodaq::verbosity::level::info }, { "warning", yaodaq::verbosity::level::warning },
    { "error", yaodaq::verbosity::level::error }, { "critical", yaodaq::verbosity::level::critical }, { "off", yaodaq::verbosity::level::off },
  };
};

}  // namespace yaodaq
