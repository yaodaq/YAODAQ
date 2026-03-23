#pragma once

#include <cstdint>
#include <string>
#include <yaodaq/Export.hpp>
namespace yaodaq
{
class WebSocketCloseConstant
{
public:
  static bool isRejected( const std::uint16_t reason )
  {
    if( reason >= NoYaodaqId && reason <= ClientWithThisNameAlreadyConnected ) return true;
    else
      return false;
  }
  YAODAQ_API static const std::uint16_t NoYaodaqId{ 3000 };
  YAODAQ_API static const std::uint16_t ClientWithThisNameAlreadyConnected{ 3001 };
  YAODAQ_API static const std::string   NoYaodaqIdMessage;
  YAODAQ_API static const std::string   ClientWithThisNameAlreadyConnectedMessage;
};
}  // namespace yaodaq
