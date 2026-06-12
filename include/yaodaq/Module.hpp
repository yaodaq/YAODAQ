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
class Module : public Client
{
public:
  YAODAQ_API Module( Config& config, const std::string_view name, const std::string_view type = "yaodaq", const Component::Role role = Component::Role::Module ) : Client( Identifier( role, type, name ), config )
  {
    Cleaner::instance().add( this );
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
    Add( "connect", jsonrpc::GetHandle( &yaodaq::Module::connect, *this ) );
    Add( "disconnect", jsonrpc::GetHandle( &yaodaq::Module::disconnect, *this ) );
  }
  Module( const Module& )            = delete;
  Module& operator=( const Module& ) = delete;
  Module( Module&& )                 = delete;
  Module& operator=( Module&& )      = delete;

  YAODAQ_API virtual bool connect()
  {
    m_State.setId( State::ID::Connected );
    return true;
  }
  YAODAQ_API virtual bool disconnect()
  {
    m_State.setId( State::ID::Disconnected );
    return true;
  }

  // Events
  YAODAQ_API bool link()
  {
    info( "Linking" );
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
      info( "Initializing" );
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
      warn( "{} to {} unauthorised", getStateStr(), "Initialized" );
      return false;
    }
  }

  YAODAQ_API bool configure()
  {
    Transition transition{ allowTransition( State::ID::Configured ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      info( "Configuring" );
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
      warn( "{} to {} unauthorised", getStateStr(), "Configured" );
      return false;
    }
  }

  YAODAQ_API bool start()
  {
    Transition transition{ allowTransition( State::ID::Started ) };
    if( transition == Transition::alreadyDone ) return true;
    if( transition != Transition::allowed )
    {
      warn( "{} to {} unauthorised", getStateStr(), "Started" );
      return false;
    }

    std::unique_lock lk( m_mutex );

    // If no run function is set, just update state and return
    if( !m_onrun )
    {
      info( "No run function set, marking module as Started without launching worker thread" );
      m_worker_state.store( WorkerState::Running );
      m_State.setId( State::ID::Started );
      return true;
    }

    // Start worker only if not running
    if( !m_worker.joinable() )
    {
      info( "Starting Module Worker" );

      m_worker_state.store( WorkerState::Running );
      m_State.setId( State::ID::Started );

      m_worker = std::thread(
        [this]()
        {
          while( true )
          {
            std::unique_lock lk( m_mutex );
            cv.wait( lk, [this]() { return m_worker_state.load() != WorkerState::Paused; } );

            if( m_worker_state.load() == WorkerState::Stopped ) break;
            lk.unlock();

            try
            {
              if( m_onrun && !m_onrun() )
              {
                error( "Run function returned false, stopping worker." );
                m_worker_state.store( WorkerState::Stopped );
                break;
              }
            }
            catch( const std::exception& e )
            {
              error( "Exception in run(): {}", e.what() );
              m_worker_state.store( WorkerState::Stopped );
              break;
            }
            catch( ... )
            {
              error( "Unknown exception in run()" );
              m_worker_state.store( WorkerState::Stopped );
              break;
            }
          }

          info( "Worker thread exiting" );
        } );
    }
    else
    {
      // Worker already running, just resume if paused
      if( m_worker_state.load() == WorkerState::Paused )
      {
        m_worker_state.store( WorkerState::Running );
        cv.notify_all();
      }
    }

    return true;
  }

  YAODAQ_API bool pause()
  {
    Transition transition{ allowTransition( State::ID::Paused ) };
    if( transition == Transition::alreadyDone ) return true;
    if( transition != Transition::allowed )
    {
      warn( "{} to {} unauthorised", getStateStr(), "Paused" );
      return false;
    }

    info( "Pausing Module" );

    // Update state safely
    {
      std::unique_lock lk( m_mutex );
      m_State.setId( State::ID::Paused );

      // Only pause worker thread if it exists
      if( m_worker.joinable() ) { m_worker_state.store( WorkerState::Paused ); }
    }

    cv.notify_all();  // Notify worker if it exists

    return true;
  }

  YAODAQ_API bool resume()
  {
    Transition transition{ allowTransition( State::ID::Started ) };
    if( transition == Transition::alreadyDone ) return true;
    if( transition != Transition::allowed )
    {
      warn( "{} to {} unauthorised", getStateStr(), "Started" );
      return false;
    }

    info( "Resuming Module" );

    {
      std::unique_lock lk( m_mutex );
      m_State.setId( State::ID::Started );

      // Only resume worker thread if it exists
      if( m_worker.joinable() ) { m_worker_state.store( WorkerState::Running ); }
    }

    cv.notify_all();  // Notify worker if it exists

    return true;
  }

  YAODAQ_API bool stop()
  {
    Transition transition{ allowTransition( State::ID::Stopped ) };
    if( transition == Transition::alreadyDone ) return true;
    if( transition != Transition::allowed )
    {
      warn( "{} to {} unauthorised", getStateStr(), "Stopped" );
      return false;
    }

    info( "Stopping Module" );

    bool ret = on_stop();  // call the hook
    if( !ret )
    {
      error( "on_stop() hook failed." );
      return false;
    }

    {
      std::unique_lock lk( m_mutex );
      m_worker_state.store( WorkerState::Stopped );
      m_State.setId( State::ID::Stopped );
    }

    cv.notify_all();  // Notify the worker thread if it exists

    // Join worker if it was started
    if( m_worker.joinable() ) { m_worker.join(); }

    return true;
  }

  YAODAQ_API bool clear()
  {
    Transition transition{ allowTransition( State::ID::Cleared ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      info( "Clearing" );
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
      warn( "{} to {} unauthorised", getStateStr(), "Cleared" );
      return false;
    }
  }

  YAODAQ_API bool release()
  {
    Transition transition{ allowTransition( State::ID::Linked ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      info( "Releasing" );
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
      warn( "{} to {} unauthorised", getStateStr(), "Linked" );
      return false;
    }
  }

  YAODAQ_API bool relink()
  {
    info( "Relinking" );
    stop();
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
  YAODAQ_API ~Module() noexcept override
  {
    debug( "~Module called" );
    Cleaner::instance().remove( this );
  }

  YAODAQ_API void setRun( const std::function<bool()>& fun ) noexcept { m_onrun = fun; }

protected:
  bool cleanup() override
  {
    info( "Module cleanup" );
    if( m_worker.joinable() )
    {
      {
        std::unique_lock lk( m_mutex );
        m_worker_state.store( WorkerState::Stopped );
      }
      cv.notify_all();
      on_stop();  // call hook for proper cleanup
      m_worker.join();
    }
    Client::cleanup();
    return true;
  }
  // link
  virtual bool pre_link() { return true; }
  virtual bool on_link() { return true; }
  virtual bool post_link() { return true; }
  // initialize
  virtual bool pre_initialize() { return true; }
  virtual bool on_initialize() { return true; }
  virtual bool post_initialize() { return true; }
  // configure
  virtual bool pre_configure() { return true; }
  virtual bool on_configure() { return true; }
  virtual bool post_configure() { return true; }
  // start
  virtual bool pre_start() { return true; }
  virtual bool on_start() { return true; }
  virtual bool post_start() { return true; }
  virtual bool on_first_start() { return true; }  // TODO
  // pause
  virtual bool pre_pause() { return true; }
  virtual bool on_pause() { return true; }
  virtual bool post_pause() { return true; }
  // resume
  virtual bool pre_resume() { return true; }
  virtual bool on_resume() { return true; }
  virtual bool post_resume() { return true; }
  // stop
  virtual bool pre_stop() { return true; }
  virtual bool on_stop() { return true; }
  virtual bool post_stop() { return true; }
  // clear
  virtual bool pre_clear() { return true; }
  virtual bool on_clear() { return true; }
  virtual bool post_clear() { return true; }
  // release
  virtual bool pre_release() { return true; }
  virtual bool on_release() { return true; }
  virtual bool post_release() { return true; }

  State      m_State;
  std::mutex m_mutex;
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
      warn( "Already in state '{}'", m_State.str() );
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
        else
          return Transition::refused;
      }
      default: return Transition::refused;
    }
  }
  void send_to_server( const std::string_view str ) { send( str ); }

private:
  enum class WorkerState : int
  {
    Running,
    Paused,
    Stopped
  };
  std::atomic<WorkerState> m_worker_state{ WorkerState::Stopped };
  std::condition_variable  cv;
  std::thread              m_worker;
  std::function<bool()>    m_onrun;
};

}  // namespace yaodaq
