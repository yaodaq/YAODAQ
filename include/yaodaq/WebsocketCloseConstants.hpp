#pragma once

#include <cstdint>
#include <string>

namespace yaodaq
{
class WebSocketCloseConstant
{
public:
  static const std::uint16_t NoYaodaqId{ 3000 };
  static const std::string   NoYaodaqIdMessage;
};
}  // namespace yaodaq
