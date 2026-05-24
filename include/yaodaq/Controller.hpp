#pragma once
#include "yaodaq/Client.hpp"
#include "yaodaq/Component.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/JsonRPCAsker.hpp"
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
class YAODAQ_API Controller : public yaodaq::Client, public JsonRPCAsker
{
public:
  YAODAQ_API Controller( const std::string_view name, ClientConfig& client_config, const std::string_view type = "yaodaq" ) : Client( Identifier( Component::Role::Controller, type, name ), client_config ) {}

  // Events
  YAODAQ_API bool link()
  {
    yaodaq::Client::start();
    return true;
  }

  YAODAQ_API ResponseClients initialize()
  {
    info( "Initializing" );
    return CallMethod( "initialize" );
  }

  YAODAQ_API ResponseClients configure()
  {
    info( "Configuring" );
    return CallMethod( "configure" );
  }

  YAODAQ_API ResponseClients start()
  {
    info( "Starting" );
    return CallMethod( "start" );
  }

  YAODAQ_API ResponseClients pause()
  {
    info( "Pausing" );
    return CallMethod( "pause" );
  }

  YAODAQ_API ResponseClients resume()
  {
    info( "Resuming" );
    return CallMethod( "resume" );
  }

  YAODAQ_API ResponseClients stop()
  {
    info( "Stopping" );
    return CallMethod( "stop" );
  }

  YAODAQ_API ResponseClients clear()
  {
    info( "Clearing" );
    return CallMethod( "clear" );
  }

  YAODAQ_API ResponseClients release()
  {
    info( "Releasing" );
    return CallMethod( "release" );
  }

  YAODAQ_API ResponseClients connect()
  {
    info( "Connecting" );
    return CallMethod( "connect" );
  }

  YAODAQ_API ResponseClients disconnect()
  {
    info( "Disconnecting" );
    return CallMethod( "disconnect" );
  }

  YAODAQ_API bool relink()
  {
    info( "Relinking" );
    stop();
    bool ret = link();
    return ret;
  }
  YAODAQ_API explicit Controller() noexcept = delete;
  YAODAQ_API virtual ~Controller() noexcept = default;

protected:
private:
  virtual void onResponse( const std::string& ) override final;
  virtual void Send( const std::string_view request ) override final;
};

}  // namespace yaodaq
