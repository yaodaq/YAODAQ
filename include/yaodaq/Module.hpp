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
    m_State.setId( State::Type::Connected );
    return true;
  }
  YAODAQ_API virtual bool disconnect()
  {
    m_State.setId( State::Type::Disconnected );
    return true;
  }

  // Events
  YAODAQ_API bool link()
  {
    info( "Linking" );
    yaodaq::Client::start();
    m_State.setId( State::Type::Linked );
    return true;
  }

  YAODAQ_API bool initialize()
  {
    Transition transition{ allowTransition( State::Type::Initialized ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      info( "Initializing" );
      bool ret = on_initialize();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::Type::Initialized );
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
    Transition transition{ allowTransition( State::Type::Configured ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      info( "Configuring" );
      bool ret = on_configure();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::Type::Configured );
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
    Transition transition{ allowTransition( State::Type::Started ) };
    if( transition == Transition::alreadyDone ) return true;
    if( transition != Transition::allowed )
    {
      warn( "{} to {} unauthorised", getStateStr(), "Started" );
      return false;
    }
    info( "Starting Module" );
    if( !on_start() ) return false;
    {
      std::scoped_lock lk( m_mutex );
      m_State.setId( State::Type::Started );
    }
    // No run callback -> only update state
    if( !m_onrun ) return true;
    // Already running?
    if( m_worker.joinable() )
    {
      info( "Worker already running" );
      return true;
    }

    info( "Launching worker thread" );
    m_worker = std::jthread(
      [this]( std::stop_token stop )
      {
        try
        {
          while( !stop.stop_requested() )
          {
            {
              std::unique_lock lk( m_mutex );

              cv.wait( lk, [this, &stop] { return stop.stop_requested() || m_worker_state != WorkerState::Paused; } );

              if( stop.stop_requested() ) break;
            }

            if( !m_onrun( stop ) )
            {
              info( "Run function requested exit." );
              break;
            }
          }
        }
        catch( const std::exception& e )
        {
          error( "Exception in run(): {}", e.what() );
        }
        catch( ... )
        {
          error( "Unknown exception in run()" );
        }

        info( "Worker thread exited" );
      } );

    m_worker_state.store( WorkerState::Running );

    return true;
  }

  YAODAQ_API bool pause()
  {
    Transition transition{ allowTransition( State::Type::Paused ) };
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
      m_State.setId( State::Type::Paused );

      // Only pause worker thread if it exists
      if( m_worker.joinable() ) { m_worker_state.store( WorkerState::Paused ); }
    }

    cv.notify_all();  // Notify worker if it exists

    return true;
  }

  YAODAQ_API bool resume()
  {
    Transition transition{ allowTransition( State::Type::Started ) };
    if( transition == Transition::alreadyDone ) return true;
    if( transition != Transition::allowed )
    {
      warn( "{} to {} unauthorised", getStateStr(), "Started" );
      return false;
    }

    info( "Resuming Module" );

    {
      std::unique_lock lk( m_mutex );
      m_State.setId( State::Type::Started );

      // Only resume worker thread if it exists
      if( m_worker.joinable() ) { m_worker_state.store( WorkerState::Running ); }
    }

    cv.notify_all();  // Notify worker if it exists

    return true;
  }

  YAODAQ_API bool stop()
  {
    Transition transition{ allowTransition( State::Type::Stopped ) };
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
      m_State.setId( State::Type::Stopped );
    }
    m_worker.request_stop();
    cv.notify_all();
    if( m_worker.joinable() ) m_worker.join();

    return true;
  }

  YAODAQ_API bool clear()
  {
    Transition transition{ allowTransition( State::Type::Cleared ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      info( "Clearing" );
      bool ret = on_clear();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::Type::Cleared );
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
    Transition transition{ allowTransition( State::Type::Released ) };
    if( transition == Transition::alreadyDone ) return true;
    else if( transition == Transition::allowed )
    {
      info( "Releasing" );
      bool ret = on_release();
      if( ret )
      {
        std::unique_lock lk( m_mutex );
        m_State.setId( State::Type::Released );
      }
      return ret;
    }
    else
    {
      warn( "{} to {} unauthorised", getStateStr(), "Released" );
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
      m_State.setId( State::Type::Linked );
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
    m_worker.request_stop();
    cv.notify_all();
    if( m_worker.joinable() ) m_worker.join();
  }

  YAODAQ_API void setRun( const std::function<bool( std::stop_token )>& fun ) noexcept { m_onrun = fun; }

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
  enum class Transition : std::uint8_t
  {
    allowed     = true,
    refused     = false,
    alreadyDone = 2,
  };
  inline static const std::unordered_map<State::Type, std::unordered_set<State::Type>> allowed = { { State::Type::Linked, { State::Type::Initialized } },
                                                                                                   { State::Type::Initialized, { State::Type::Connected, State::Type::Released } },
                                                                                                   { State::Type::Connected, { State::Type::Configured, State::Type::Disconnected } },
                                                                                                   { State::Type::Configured, { State::Type::Started, State::Type::Cleared } },
                                                                                                   { State::Type::Started, { State::Type::Paused, State::Type::Stopped } },
                                                                                                   { State::Type::Paused, { State::Type::Stopped, State::Type::Started } },
                                                                                                   { State::Type::Stopped, { State::Type::Started, State::Type::Cleared } },
                                                                                                   { State::Type::Cleared, { State::Type::Disconnected, State::Type::Configured } },
                                                                                                   { State::Type::Disconnected, { State::Type::Connected, State::Type::Released } },
                                                                                                   { State::Type::Released, { State::Type::Initialized } } };
  Transition                                                                           allowTransition( const State::Type to )
  {
    const State::Type current{ getState().type() };
    if( to == current )
    {
      warn( "Already in state '{}'", m_State.str() );
      return Transition::alreadyDone;
    }
    const auto it = allowed.find( current );
    if( it == allowed.end() ) return Transition::refused;
    return it->second.contains( to ) ? Transition::allowed : Transition::refused;
  }

  void send_to_server( const std::string_view str ) { send( str ); }

private:
  enum class WorkerState : std::uint8_t
  {
    Running,
    Paused,
    Stopped
  };
  std::atomic<WorkerState>               m_worker_state{ WorkerState::Stopped };
  std::condition_variable                cv;
  std::jthread                           m_worker;
  std::function<bool( std::stop_token )> m_onrun;
};

}  // namespace yaodaq
