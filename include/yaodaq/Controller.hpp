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
    m_State.setId( State::ID::Linked );
    return true;
  }

  YAODAQ_API ResponseClients initialize()
  {
    logger()->info( "Initializing" );
    return CallMethod( "initialize" );
    m_State.setId( State::ID::Initialized );
  }

  YAODAQ_API ResponseClients configure()
  {
    logger()->info( "Configuring" );
    return CallMethod( "configure" );
    m_State.setId( State::ID::Configured );
  }

  YAODAQ_API ResponseClients start()
  {
    logger()->info( "Starting" );
    return CallMethod( "start" );
  }

  YAODAQ_API ResponseClients pause()
  {
    logger()->info( "Pausing" );
    return CallMethod( "pause" );
  }

  YAODAQ_API ResponseClients resume() { return CallMethod( "resume" ); }

  YAODAQ_API ResponseClients stop()
  {
    logger()->info( "Stopping" );
    return CallMethod( "stop" );
  }

  YAODAQ_API ResponseClients clear()
  {
    logger()->info( "Clearing" );
    return CallMethod( "clear" );
  }

  YAODAQ_API ResponseClients release() { return CallMethod( "release" ); }

  YAODAQ_API ResponseClients connect() { return CallMethod( "connect" ); }

  YAODAQ_API ResponseClients disconnect() { return CallMethod( "disconnect" ); }

  YAODAQ_API bool relink()
  {
    logger()->info( "Relinking" );
    yaodaq::Client::stop();
    bool ret = link();
    m_State.setId( State::ID::Linked );
    return ret;
  }

  YAODAQ_API State getState() const noexcept { return m_State; }

  YAODAQ_API std::string getStateStr() { return m_State.str(); }

  YAODAQ_API explicit Controller() noexcept = delete;
  YAODAQ_API virtual ~Controller() noexcept {}

protected:
  virtual bool on_initialize() { return true; };
  virtual bool on_configure() { return true; };
  virtual bool on_start()
  {
    logger()->info( "on_start running" );
    return true;
  };
  virtual bool on_pause() { return true; };
  virtual bool on_resume() { return true; };
  virtual bool on_stop() { return true; };
  virtual bool on_clear() { return true; };
  virtual bool on_release() { return true; };
  State        m_State;

private:
  virtual void onResponse( const std::string& ) override final;
  virtual void Send( const std::string_view request ) override final;
};

}  // namespace yaodaq
