#pragma once
#include "yaodaq/Client.hpp"
#include "yaodaq/Component.hpp"
#include "yaodaq/Export.hpp"

#include <cstdint>
#include <string_view>

namespace yaodaq
{

/**
* @brief A YAODAQ Module is a generic class for all class that not need to connect
*
**/
class Module : public yaodaq::Client
{
public:
  YAODAQ_API Module( const std::string_view name, const std::string_view host = defaults::host, const std::uint16_t port = defaults::port, const std::string_view path = defaults::path, const std::string_view type = "UserType" ) :
    Client( { Component::role::Module, type, name }, host, port, path )
  {
  }
  YAODAQ_API explicit Module() noexcept = delete;
  YAODAQ_API virtual ~Module() noexcept = default;
};

}  // namespace yaodaq
