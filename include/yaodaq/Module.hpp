#pragma once
#include "yaodaq/Client.hpp"
#include "yaodaq/Component.hpp"
#include "yaodaq/Export.hpp"
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
* @brief A YAODAQ Module is a generic class for all class that not need to connect
*
**/
class Module : public yaodaq::Client
{
public:
  YAODAQ_API Module( const std::string_view name, const std::string_view host = defaults::host, const std::uint16_t port = defaults::port, const std::string_view path = defaults::path, const Component::Role role = Component::Role::Module,
                     const std::string_view type = "UserType" ) : Client( { role, type, name }, host, port, path )
  {
    Add( "initialize", jsonrpc::GetHandle( &yaodaq::Module::initialize, *this ) );
    Add( "configure", jsonrpc::GetHandle( &yaodaq::Module::configure, *this ) );
    Add( "start", jsonrpc::GetHandle( &yaodaq::Module::start, *this ) );
    Add( "stop", jsonrpc::GetHandle( &yaodaq::Module::stop, *this ) );
    Add( "pause", jsonrpc::GetHandle( &yaodaq::Module::pause, *this ) );
    Add( "resume", jsonrpc::GetHandle( &yaodaq::Module::resume, *this ) );
    Add( "clear", jsonrpc::GetHandle( &yaodaq::Module::clear, *this ) );
    Add( "release", jsonrpc::GetHandle( &yaodaq::Module::release, *this ) );
    Add( "relink", jsonrpc::GetHandle( &yaodaq::Module::relink, *this ) );
    Add( "getState", jsonrpc::GetHandle( &yaodaq::Module::getStateStr, *this ) );
  }
  Module( const Module& )               = delete;
  Module& operator=( const Module& )    = delete;
  Module( Module&& )                    = delete;
  Module&         operator=( Module&& ) = delete;
  // Events
  YAODAQ_API bool link()
  {
    logger()->info( "Linking" );
    yaodaq::Client::start();
    m_State.setId( State::ID::Linked );
    return true;
  }

  YAODAQ_API bool initialize()
  {
    Transition transition{ allowTransition( State::ID::Initialized ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      logger()->info( "Initializing" );
      bool ret = on_initialize();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::ID::Initialized );
      }
      return ret;
    }
    else
    {
      logger()->warn( "{} to {} unauthorised", getStateStr(), "Initialized" );
      return false;
    }
  }

  YAODAQ_API bool configure()
  {
    Transition transition{ allowTransition( State::ID::Configured ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      logger()->info( "Configuring" );
      bool ret = on_configure();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::ID::Configured );
      }
      return ret;
    }
    else
    {
      logger()->warn( "{} to {} unauthorised", getStateStr(), "Configured" );
      return false;
    }
  }

  YAODAQ_API bool start()
  {
    Transition transition{ allowTransition( State::ID::Started ) };
    if( transition == Transition::alreadyDone ) return true;
    if( transition != Transition::allowed )
    {
      logger()->warn( "{} to {} unauthorised", getStateStr(), "Started" );
      return false;
    }

    // Lock to safely check and modify shared state
    {
      std::unique_lock lk( m_mutex );

      if( m_worker.joinable() )
      {
        logger()->warn( "Worker thread is already running." );
        return true;
      }

      logger()->info( "Starting" );

      if( !on_first_start() ) return false;

      if( !on_start() )
      {
        logger()->error( "on_start() failed." );
        return false;
      }

      // Reset state variables safely under lock
      m_stopped.store( false );
      m_paused.store( false );
      m_event_nbr.store( 0 );

      // Update module state under lock
      m_State.setId( State::ID::Started );
    }

    // Launch worker thread outside the lock
    m_worker = std::thread(
      [this]()
      {
        while( !m_stopped.load() )
        {
          std::unique_lock lk( m_mutex );

          // Wait until not paused or stopped; optional timeout to avoid deadlock
          cv.wait_for( lk, std::chrono::seconds( 1 ), [this]() { return !m_paused.load() || m_stopped.load(); } );

          if( m_stopped.load() )
          {
            logger()->info( "Worker thread stopped." );
            break;
          }

          lk.unlock();  // Release lock during run()

          try
          {
            if( !run() )
            {
              logger()->error( "Run failed, stopping worker thread." );
              m_stopped.store( true );
              break;
            }
            m_event_nbr++;
          }
          catch( const std::exception& e )
          {
            logger()->error( "Exception in run(): {}", e.what() );
            m_stopped.store( true );
            m_paused.store( false );
            break;
          }
          catch( ... )
          {
            logger()->error( "Unknown exception in run()" );
            m_stopped.store( true );
            m_paused.store( false );
            break;
          }
        }
      } );

    return true;
  }

  YAODAQ_API bool pause()
  {
    Transition transition{ allowTransition( State::ID::Paused ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      logger()->info( "Pausing" );
      bool ret = on_pause();
      if( ret )
      {
        {
          std::unique_lock lk( m_mutex );
          m_State.setId( State::ID::Paused );
          m_paused.store( true );
        }
        cv.notify_all();  // Notify the worker thread to check the pause state
      }
      return ret;
    }
    else
    {
      logger()->warn( "{} to {} unauthorised", getStateStr(), "Paused" );
      return false;
    }
  }

  YAODAQ_API bool resume()
  {
    Transition transition{ allowTransition( State::ID::Started ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      logger()->info( "Resuming" );
      bool ret = on_resume();
      if( ret )
      {
        {
          std::unique_lock lk( m_mutex );
          m_paused.store( false );
          m_State.setId( State::ID::Started );
        }
        cv.notify_all();
      }
      return ret;
    }
    else
    {
      logger()->warn( "{} to {} unauthorised", getStateStr(), "Started" );
      return false;
    }
  }

  YAODAQ_API bool stop()
  {
    Transition transition{ allowTransition( State::ID::Stopped ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      logger()->info( "Stopping" );
      bool ret = on_stop();
      if( ret )
      {
        m_stopped.store( true );
        m_paused.store( false );
        m_event_nbr.store( 0 );
        cv.notify_all();
        if( m_worker.joinable() ) { m_worker.join(); }
        {
          std::unique_lock lk( m_mutex );
          m_State.setId( State::ID::Stopped );
        }
      }
      return ret;
    }
    else
    {
      logger()->warn( "{} to {} unauthorised", getStateStr(), "Stopped" );
      return false;
    }
  }

  YAODAQ_API bool clear()
  {
    Transition transition{ allowTransition( State::ID::Cleared ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      logger()->info( "Clearing" );
      bool ret = on_clear();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::ID::Cleared );
      }
      return ret;
    }
    else
    {
      logger()->warn( "{} to {} unauthorised", getStateStr(), "Cleared" );
      return false;
    }
  }

  YAODAQ_API bool release()
  {
    Transition transition{ allowTransition( State::ID::Linked ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      logger()->info( "Releasing" );
      bool ret = on_release();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::ID::Linked );
      }
      return ret;
    }
    else
    {
      logger()->warn( "{} to {} unauthorised", getStateStr(), "Linked" );
      return false;
    }
  }

  YAODAQ_API bool relink()
  {
    logger()->info( "Relinking" );
    yaodaq::Client::stop();
    bool ret = link();
    if( ret )
    {
      std::unique_lock lk( m_mutex );
      m_State.setId( State::ID::Linked );
    }
    return ret;
  }

  YAODAQ_API State getState() noexcept
  {
    std::scoped_lock lk( m_mutex );
    return m_State;
  }

  YAODAQ_API std::string getStateStr()
  {
    std::scoped_lock lk( m_mutex );
    return m_State.str();
  }

  YAODAQ_API explicit Module() noexcept = delete;
  YAODAQ_API virtual ~Module() noexcept
  {
    if( m_worker.joinable() )
    {
      {
        std::unique_lock lk( m_mutex );
        m_stopped.store( true );
        m_paused.store( false );
      }
      cv.notify_all();
      on_stop();  // call hook for proper cleanup
      m_worker.join();
    }
  }

protected:
  virtual bool on_initialize() { return true; };
  virtual bool on_configure() { return true; };
  virtual bool on_first_start() { return true; }
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
  virtual bool run()
  {
    logger()->info( "running... event {}", m_event_nbr.load() );
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
    return true;
  }
  State m_State;

private:
  enum class Transition : int
  {
    allowed     = true,
    refused     = false,
    alreadyDone = 2,
  };
  Transition allowTransition( const State::ID& state )
  {
    if( state == getState().id() )
    {
      logger()->warn( "Already in state '{}'", m_State.str() );
      return Transition::alreadyDone;
    }
    switch( state )
    {
      case State::ID::Initialized:
      {
        if( getState().id() == State::ID::Linked ) return Transition::allowed;
        else if( getState().id() == State::ID::Released )
          return Transition::allowed;
        else
          return Transition::refused;
      }
      case State::ID::Connected:
      {
        if( getState().id() == State::ID::Initialized ) return Transition::allowed;
        else
          return Transition::refused;
      }
      case State::ID::Configured:
      {
        if( getState().id() == State::ID::Connected ) return Transition::allowed;
        else if( getState().id() == State::ID::Initialized && m_identifier.component().role() != Component::Role::Board )
          return Transition::allowed;
        else
          return Transition::refused;
      }
      case State::ID::Started:
      {
        if( getState().id() == State::ID::Configured ) return Transition::allowed;
        else if( getState().id() == State::ID::Paused )
          return Transition::allowed;
        else if( getState().id() == State::ID::Stopped )
          return Transition::allowed;
        else
          return Transition::refused;
      }
      case State::ID::Paused:
      {
        if( getState().id() == State::ID::Started ) return Transition::allowed;
        else
          return Transition::refused;
      }
      case State::ID::Stopped:
      {
        if( getState().id() == State::ID::Started ) return Transition::allowed;
        else if( getState().id() == State::ID::Paused )
          return Transition::allowed;
        else
          return Transition::refused;
      }
      case State::ID::Cleared:
      {
        if( getState().id() == State::ID::Stopped ) return Transition::allowed;
        else
          return Transition::refused;
      }
      case State::ID::Disconnected:
      {
        if( getState().id() == State::ID::Cleared ) return Transition::allowed;
        else
          return Transition::refused;
      }
      case State::ID::Released:
      {
        if( getState().id() == State::ID::Disconnected ) return Transition::allowed;
        else if( getState().id() == State::ID::Cleared && m_identifier.component().role() != Component::Role::Board )
          return Transition::allowed;
        else
          return Transition::refused;
      }
      default: return Transition::refused;
    }
  }
  std::atomic<bool>          m_paused{ false };
  std::atomic<bool>          m_stopped{ false };
  std::mutex                 m_mutex;
  std::condition_variable    cv;
  std::thread                m_worker;
  std::atomic<std::uint64_t> m_event_nbr{ 0 };
};

}  // namespace yaodaq
