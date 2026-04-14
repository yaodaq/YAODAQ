#pragma once
#include "Module.hpp"

#include <cstdint>
#include <mutex>
#include <string_view>
#include <yaodaq/Defaults.hpp>
#include <yaodaq/Export.hpp>
#include <yaodaq/Identifier.hpp>

namespace yaodaq
{

/**
* @brief A YAODAQ Board is a Module who need to connect
*
**/
class Board : public Module
{
public:
  YAODAQ_API explicit Board( ClientConfig& cfg, const std::string_view name, const std::string_view type = "yaodaq" ) : Module( cfg, name, type, Component::Role::Board )
  {
    Add( "connect", jsonrpc::GetHandle( &yaodaq::Board::connect, *this ) );
    Add( "disconnect", jsonrpc::GetHandle( &yaodaq::Board::disconnect, *this ) );
  }
  bool connect()
  {
    Transition transition{ allowTransition( State::ID::Connected ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      logger()->info( "Connecting" );
      bool ret = on_connect();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::ID::Connected );
      }
      return ret;
    }
    else
    {
      logger()->warn( "{} to {} unauthorised", getStateStr(), "Connected" );
      return false;
    }
  };
  bool disconnect()
  {
    Transition transition{ allowTransition( State::ID::Disconnected ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      logger()->info( "Disconnecting" );
      bool ret = on_disconnect();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::ID::Disconnected );
      }
      return ret;
    }
    else
    {
      logger()->warn( "{} to {} unauthorised", getStateStr(), "Disconnected" );
      return false;
    }
  };
  YAODAQ_API explicit Board() noexcept = delete;
  YAODAQ_API virtual ~Board() noexcept {}

protected:
  virtual bool on_connect() { return true; };
  virtual bool on_disconnect() { return true; };

private:
};

}  // namespace yaodaq
