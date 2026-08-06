#pragma once
#include "yaodaq/Connector.hpp"
#include "yaodaq/Defaults.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Identifier.hpp"
#include "yaodaq/Module.hpp"

#include <future>
#include <memory>
#include <string_view>

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

  YAODAQ_API ReturnValue connect() noexcept final
  {
    try
    {
      m_connector->setCodecParameters( m_config.codecParameters() );
      m_connector->setTransportParameters( m_config.transportParameters() );
      m_connector->setLogger( this->get_logger() );
      const Transition transition{ allowTransition( State::Type::Connected ) };
      if( !shouldExecute( transition ) ) return isSuccess( transition );
      info( "Connecting" );
      bool good{ true };
      good = pre_connect( transition == Transition::alreadyDone );
      if( !good ) return ReturnValue( "pre_connect() failed" );
      good = on_connect();
      if( !good ) return ReturnValue( "on_connect() failed" );
      updateState( State::Type::Connected );
      good = post_connect();
      if( !good ) return ReturnValue( "post_connect() failed" );
      return ReturnValue( true );
    }
    catch( const std::exception& ex )
    {
      error( "error while configuring: {}", ex.what() );
      return ReturnValue( ex );
    }
    catch( ... )
    {
      error( "error while configuring" );
      return ReturnValue::fromException();
    }
  };

  YAODAQ_API ReturnValue disconnect() noexcept final
  {
    try
    {
      const Transition transition{ allowTransition( State::Type::Disconnected ) };
      if( !shouldExecute( transition ) ) return isSuccess( transition );
      info( "Disconnecting" );
      bool good{ true };
      good = pre_disconnect( transition == Transition::alreadyDone );
      if( !good ) return ReturnValue( "pre_disconnect() failed" );
      good = on_disconnect();
      if( !good ) return ReturnValue( "on_disconnect() failed" );
      updateState( State::Type::Disconnected );
      good = post_disconnect();
      if( !good ) return ReturnValue( "post_disconnect() failed" );
      return ReturnValue( true );
    }
    catch( const std::exception& ex )
    {
      error( "error while configuring: {}", ex.what() );
      return ReturnValue( ex );
    }
    catch( ... )
    {
      error( "error while configuring" );
      return ReturnValue::fromException();
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
  virtual bool            pre_connect( const bool alreadyDone ) { return true; };
  virtual bool            post_connect() { return true; };
  virtual bool            pre_disconnect( const bool alreadyDone ) { return true; };
  virtual bool            post_disconnect() { return true; };
  YAODAQ_API virtual bool cleanup() final
  {
    debug( "Board cleanup" );
    disconnect();
    Module::cleanup();
    return true;
  }

private:
  bool                       on_connect() { return m_connector->connect(); };
  bool                       on_disconnect() { return m_connector->disconnect(); };
  std::unique_ptr<Connector> m_connector{ nullptr };
  BoardConfig                m_config;
};

}  // namespace yaodaq
