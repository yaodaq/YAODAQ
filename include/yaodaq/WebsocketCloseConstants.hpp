#pragma once

#include <cstdint>
#include <string>
#include <yaodaq/Export.hpp>
namespace yaodaq
{
class WebSocketCloseConstant
{
public:
  YAODAQ_API static const std::uint16_t NoYaodaqId{ 3000 };
  YAODAQ_API static const std::string   NoYaodaqIdMessage;
};
}  // namespace yaodaq
