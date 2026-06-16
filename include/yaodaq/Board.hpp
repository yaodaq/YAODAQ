#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/Module.hpp"

#include <cstdint>
#include <mutex>
#include <string_view>
#include <yaodaq/Connector.hpp>
#include <yaodaq/Defaults.hpp>
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
  Board( BoardConfig& cfg, const std::string_view name, const std::string_view type = "yaodaq" ) : Module( cfg, name, type, Component::Role::Board ), m_config( std::move( cfg ) )
  {
    Cleaner::instance().add( this );
    m_connector = m_config.takeConnector();
  }
  YAODAQ_API        Board( const Board& ) noexcept     = delete;
  YAODAQ_API Board& operator=( const Board& ) noexcept = delete;
  YAODAQ_API        Board( Board&& ) noexcept          = delete;
  YAODAQ_API Board& operator=( Board&& ) noexcept      = delete;
  YAODAQ_API bool   connect() final
  {
    m_connector->setCodecParameters( m_config.codecParameters() );
    m_connector->setTransportParameters( m_config.transportParameters() );
    m_connector->setLogger( this->get_logger() );
    Transition transition{ allowTransition( State::Type::Connected ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      info( "Connecting" );
      bool ret = on_connect();
      if( !ret ) return false;
      m_connector->setLogger( this->get_logger() );
      ret = m_connector->connect();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::Type::Connected );
      }
      return ret;
    }
    else
    {
      warn( "{} to {} unauthorised", getStateStr(), "Connected" );
      return false;
    }
  };
  YAODAQ_API bool disconnect() final
  {
    Transition transition{ allowTransition( State::Type::Disconnected ) };
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
        m_State.setId( State::Type::Disconnected );
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
  YAODAQ_API virtual ~Board() noexcept
  {
    debug( "~Board called" );
    Cleaner::instance().remove( this );
  }
  YAODAQ_API void send_to_device( std::unique_ptr<Message> msg ) { m_connector->send( std::move( msg ) ); }

  YAODAQ_API std::future<std::unique_ptr<Message>> request( std::unique_ptr<Message> msg ) { return m_connector->request( std::move( msg ) ); }

  YAODAQ_API Dispatcher& dispatcher() { return m_connector->dispatcher(); }

protected:
  virtual bool            on_connect() { return true; };
  virtual bool            on_disconnect() { return true; };
  YAODAQ_API virtual bool cleanup() final
  {
    debug( "Board cleanup" );
    disconnect();
    Module::cleanup();
    return true;
  }

private:
  std::unique_ptr<Connector> m_connector{ nullptr };
  BoardConfig                m_config;
};

}  // namespace yaodaq
