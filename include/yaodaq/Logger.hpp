#pragma once
#include "yaodaq/Component.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/JsonRPCAsker.hpp"
#include "yaodaq/Module.hpp"
#include "yaodaq/State.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <string_view>
#include <thread>

namespace yaodaq
{

/**
  * @brief A YAODAQ Controller is a generic class for all class that controls modules
  *
  **/
class YAODAQ_API Logger : public yaodaq::Module
{
public:
  YAODAQ_API Logger( const std::string_view name, const std::string_view host = defaults::host, const std::uint16_t port = defaults::port, const std::string_view path = defaults::path, const std::string_view type = "UserType" ) :
    Module( name, host, port, type, Component::Role::Logger )
  {
  }
};

}  // namespace yaodaq
