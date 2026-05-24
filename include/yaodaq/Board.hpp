#pragma once
#include "Module.hpp"

#include <cstdint>
#include <mutex>
#include <string_view>
#include <yaodaq/Connector.hpp>
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
  YAODAQ_API explicit Board( BoardConfig& cfg, const std::string_view name, const std::string_view type = "yaodaq" ) : Module( cfg, name, type, Component::Role::Board ), m_connector( cfg.getConnector() ) {}
  bool connect() final
  {
    Transition transition{ allowTransition( State::ID::Connected ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      info( "Connecting" );
      bool ret = on_connect();
      if( !ret ) return false;
      m_connector->setLogger( this );
      ret = m_connector->connect();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::ID::Connected );
      }
      return ret;
    }
    else
    {
      warn( "{} to {} unauthorised", getStateStr(), "Connected" );
      return false;
    }
  };
  bool disconnect() final
  {
    Transition transition{ allowTransition( State::ID::Disconnected ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      info( "Disconnecting" );
      bool ret = on_disconnect();
      if( !ret ) return false;
      ret = m_connector->disconnect();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::ID::Disconnected );
      }
      return ret;
    }
    else
    {
      warn( "{} to {} unauthorised", getStateStr(), "Disconnected" );
      return false;
    }
  };
  YAODAQ_API explicit Board() noexcept = delete;
  YAODAQ_API virtual ~Board() noexcept { disconnect(); }
  YAODAQ_API void send( const Message& msg ) { m_connector->send( msg ); }

  YAODAQ_API std::future<Message> request( const Message& msg ) { return m_connector->request( msg ); }

  YAODAQ_API Dispatcher& dispatcher() { return m_connector->dispatcher(); }

protected:
  virtual bool on_connect() { return true; };
  virtual bool on_disconnect() { return true; };

private:
  std::unique_ptr<Connector> m_connector{ nullptr };
};

}  // namespace yaodaq
