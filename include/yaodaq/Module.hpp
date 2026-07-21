#pragma once
#include "yaodaq/Client.hpp"
#include "yaodaq/Component.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/State.hpp"
#include "yaodaq/Utilities.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <limits>
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
    Add( "setMaxNumberEvents", jsonrpc::GetHandle( &yaodaq::Module::setMaxNumberEvents, *this ) );
  }
  Module( const Module& )            = delete;
  Module& operator=( const Module& ) = delete;
  Module( Module&& )                 = delete;
  Module& operator=( Module&& )      = delete;

  YAODAQ_API virtual bool connect()
  {
    updateState( State::Type::Connected );
    return true;
  }
  YAODAQ_API virtual bool disconnect()
  {
    updateState( State::Type::Disconnected );
    return true;
  }

  // Events
  YAODAQ_API bool link()
  {
    info( "Linking" );
    Transition transition{ allowTransition( State::Type::Linked ) };
    if( transition == Transition::alreadyDone ) return true;
    if( transition != Transition::allowed )
    {
      warn( "{} to {} unauthorised", getStateStr(), "Linked" );
      return false;
    }
    yaodaq::Client::start();
    updateState( State::Type::Linked );
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
      if( ret ) { updateState( State::Type::Initialized ); }
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
      if( ret ) { updateState( State::Type::Configured ); }
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

    {
      updateState( State::Type::Started );
      m_worker_state.store( WorkerState::Running );
    }

    // Start worker first
    if( m_onrun && !m_worker.joinable() )
    {
      info( "Launching worker thread" );

      m_worker = std::jthread(
        [this]( std::stop_token stop )
        {
          try
          {
            auto last_report = std::chrono::steady_clock::now();
            while( !stop.stop_requested() )
            {
              {
                std::unique_lock lk( m_mutex );

                cv.wait( lk, [this, &stop] { return stop.stop_requested() || m_worker_state != WorkerState::Paused; } );

                if( stop.stop_requested() ) break;
              }

              if( !m_onrun( stop ) ) { error( "on_run failed!" ); }
              else
              {
                const auto count = m_event.fetch_add( 1, std::memory_order_relaxed ) + 1;
                // Update progress every 500 ms
                const auto now   = std::chrono::steady_clock::now();
                if( now - last_report >= std::chrono::milliseconds( 500 ) )
                {
                  last_report = now;
                  info( "Events {}", progressBar( count, m_max_event ) );
                }
                if( count >= m_max_event )
                {
                  info( "Maximum number of events ({}) reached.", m_max_event.load() );
                  {
                    m_worker_state.store( WorkerState::Stopped );
                    updateState( State::Type::Finished );
                    m_event.store( 0 );
                  }
                  break;
                }
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
    }

    // Now the worker exists
    if( !on_start() )
    {
      error( "on_start() failed" );

      if( m_worker.joinable() )
      {
        m_worker.request_stop();
        cv.notify_all();
        m_worker.join();
      }

      updateState( State::Type::Configured );

      return false;
    }

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
      updateState( State::Type::Paused );

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
      updateState( State::Type::Started );

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
    m_event.store( 0, std::memory_order_relaxed );
    m_worker.request_stop();
    cv.notify_all();
    if( m_worker.joinable() ) m_worker.join();
    {
      m_worker_state.store( WorkerState::Stopped );
      updateState( State::Type::Stopped );
    }
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
      if( ret ) { updateState( State::Type::Cleared ); }
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
      if( ret ) { updateState( State::Type::Released ); }
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
    if( ret ) { updateState( State::Type::Linked ); }
    return ret;
  }

  YAODAQ_API State getState() noexcept
  {
    std::scoped_lock lock( m_mutex );
    return m_State;
  }

  YAODAQ_API std::string getStateStr()
  {
    std::scoped_lock lock( m_mutex );
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
  void            setMaxNumberEvents( std::uint64_t max ) noexcept { m_max_event.store( max, std::memory_order_relaxed ); }

protected:
  std::uint64_t event() { return m_event.load(); }
  void          reset_event() { m_event.store( 0 ); }
  void          updateState( const State::Type type )
  {
    State state( State::Type::Empty );

    {
      std::scoped_lock lock( m_mutex );
      m_State.setId( type );
      state = m_State;
    }

    send( StateUpdate( state ) );
  }
  bool cleanup() override
  {
    info( "Module cleanup" );
    if( m_worker.joinable() )
    {
      {
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

  State      m_State{ State::Type::Empty };
  std::mutex m_mutex;
  enum class Transition : std::uint8_t
  {
    allowed     = true,
    refused     = false,
    alreadyDone = 2,
  };
  inline static const std::unordered_map<State::Type, std::unordered_set<State::Type>> allowed = { { State::Type::Empty, { State::Type::Linked } },
                                                                                                   { State::Type::Linked, { State::Type::Initialized } },
                                                                                                   { State::Type::Initialized, { State::Type::Connected, State::Type::Released } },
                                                                                                   { State::Type::Connected, { State::Type::Configured, State::Type::Disconnected } },
                                                                                                   { State::Type::Configured, { State::Type::Started, State::Type::Cleared } },
                                                                                                   { State::Type::Started, { State::Type::Paused, State::Type::Stopped } },
                                                                                                   { State::Type::Paused, { State::Type::Stopped, State::Type::Started } },
                                                                                                   { State::Type::Stopped, { State::Type::Started, State::Type::Cleared } },
                                                                                                   { State::Type::Finished, { State::Type::Started, State::Type::Cleared } },
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

  void          send_to_server( const std::string_view str ) { send( str ); }
  std::uint64_t getMaxNumberEvents() const noexcept { return m_max_event.load( std::memory_order_relaxed ); }

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
  std::atomic<std::uint64_t>             m_event{ 0 };
  std::atomic<std::uint64_t>             m_max_event{ ( std::numeric_limits<std::uint64_t>::max )() };
};

}  // namespace yaodaq
