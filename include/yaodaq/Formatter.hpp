#pragma once
#include "yaodaq/Export.hpp"

#include <cstddef>
#include <nlohmann/json_fwd.hpp>

namespace yaodaq
{

class Formatter
{
public:
  YAODAQ_API static std::string format( const nlohmann::json& json, const std::size_t pad = 0 );
};

}  // namespace yaodaq
