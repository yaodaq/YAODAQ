#pragma once
#include "yaodaq/Client.hpp"
#include "yaodaq/Component.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/ReturnValue.hpp"
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
    Add( "setMaxEvents", jsonrpc::GetHandle( &yaodaq::Module::setMaxEvents, *this ) );
    Add( "getMaxEvents", jsonrpc::GetHandle( &yaodaq::Module::getMaxEvents, *this ) );
  }
  Module( const Module& )            = delete;
  Module& operator=( const Module& ) = delete;
  Module( Module&& )                 = delete;
  Module& operator=( Module&& )      = delete;

  YAODAQ_API virtual ReturnValue connect() noexcept
  {
    try
    {
      updateState( State::Type::Connected );
      return ReturnValue( true );
    }
    catch( const std::exception& ex )
    {
      error( "error while linking: {}", ex.what() );
      return ReturnValue( ex );
    }
    catch( ... )
    {
      error( "error while linking" );
      return ReturnValue::fromException();
    }
  }

  YAODAQ_API virtual ReturnValue disconnect() noexcept
  {
    try
    {
      updateState( State::Type::Disconnected );
      return ReturnValue( true );
    }
    catch( const std::exception& ex )
    {
      error( "error while linking: {}", ex.what() );
      return ReturnValue( ex );
    }
    catch( ... )
    {
      error( "error while linking" );
      return ReturnValue::fromException();
    }
  }

  YAODAQ_API ReturnValue link() noexcept
  {
    try
    {
      const Transition transition = allowTransition( State::Type::Linked );
      if( !shouldExecute( transition ) ) return isSuccess( transition );
      info( "Linking" );
      bool good{ true };
      good = pre_link( transition == Transition::alreadyDone );
      if( !good ) return ReturnValue( "pre_link() failed" );
      good = on_link();
      if( !good ) return ReturnValue( "on_link() failed" );
      updateState( State::Type::Linked );
      good = post_link();
      if( !good ) return ReturnValue( "post_link() failed" );
      return ReturnValue( true );
    }
    catch( const std::exception& ex )
    {
      error( "error while linking: {}", ex.what() );
      return ReturnValue( ex );
    }
    catch( ... )
    {
      error( "error while linking" );
      return ReturnValue::fromException();
    }
  }

  YAODAQ_API ReturnValue initialize() noexcept
  {
    try
    {
      const Transition transition{ allowTransition( State::Type::Initialized ) };
      if( !shouldExecute( transition ) ) return isSuccess( transition );
      info( "Initializing" );
      bool good{ true };
      good = pre_initialize( transition == Transition::alreadyDone );
      if( !good ) return ReturnValue( "pre_initialize() failed" );
      good = on_initialize();
      if( !good ) return ReturnValue( "on_initialize() failed" );
      updateState( State::Type::Initialized );
      good = post_initialize();
      if( !good ) return ReturnValue( "post_initialize() failed" );
      return ReturnValue( true );
    }
    catch( const std::exception& ex )
    {
      error( "error while initializing: {}", ex.what() );
      return ReturnValue( ex );
    }
    catch( ... )
    {
      error( "error while initializing" );
      return ReturnValue::fromException();
    }
  }

  YAODAQ_API ReturnValue configure()
  {
    try
    {
      const Transition transition{ allowTransition( State::Type::Configured ) };
      if( !shouldExecute( transition ) ) return isSuccess( transition );
      info( "Configuring" );
      bool good{ true };
      good = pre_configure( transition == Transition::alreadyDone );
      if( !good ) return ReturnValue( "pre_configure() failed" );
      good = on_configure();
      if( !good ) return ReturnValue( "on_configure() failed" );
      updateState( State::Type::Configured );
      good = post_configure();
      if( !good ) return ReturnValue( "post_configure() failed" );
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
  }

  YAODAQ_API bool start()
  {
    const Transition transition{ allowTransition( State::Type::Started ) };
    if( !shouldExecute( transition ) ) return isSuccess( transition );
    info( "Starting" );
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
    const Transition transition{ allowTransition( State::Type::Paused ) };
    if( !shouldExecute( transition ) ) return isSuccess( transition );
    info( "Pausing" );
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
    const Transition transition{ allowTransition( State::Type::Started ) };
    if( !shouldExecute( transition ) ) return isSuccess( transition );
    info( "Resuming " );
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
    const Transition transition{ allowTransition( State::Type::Stopped ) };
    if( !shouldExecute( transition ) ) return isSuccess( transition );
    info( "Stopping" );
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
    const Transition transition{ allowTransition( State::Type::Cleared ) };
    if( !shouldExecute( transition ) ) return isSuccess( transition );
    info( "Clearing" );
    bool ret = on_clear();
    if( ret ) { updateState( State::Type::Cleared ); }
    return ret;
  }

  YAODAQ_API bool release()
  {
    const Transition transition{ allowTransition( State::Type::Released ) };
    if( !shouldExecute( transition ) ) return isSuccess( transition );
    info( "Releasing" );
    bool ret = on_release();
    if( ret ) { updateState( State::Type::Released ); }
    return ret;
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
  std::size_t     setMaxEvents( std::uint64_t max ) noexcept
  {
    m_max_event.store( max, std::memory_order_relaxed );
    return getMaxEvents();
  }

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
  virtual bool pre_link( const bool alreadyDone ) { return true; }
  virtual bool on_link()
  {
    yaodaq::Client::start();
    return true;
  }
  virtual bool post_link() { return true; }
  // initialize
  virtual bool pre_initialize( const bool alreadyDone ) { return true; }
  virtual bool on_initialize() { return true; }
  virtual bool post_initialize() { return true; }
  // configure
  virtual bool pre_configure( const bool alreadyDone ) { return true; }
  virtual bool on_configure() { return true; }
  virtual bool post_configure() { return true; }
  // start
  virtual bool pre_start( const bool alreadyDone ) { return true; }
  virtual bool on_start() { return true; }
  virtual bool post_start() { return true; }
  // pause
  virtual bool pre_pause( const bool alreadyDone ) { return true; }
  virtual bool on_pause() { return true; }
  virtual bool post_pause() { return true; }
  // resume
  virtual bool pre_resume( const bool alreadyDone ) { return true; }
  virtual bool on_resume() { return true; }
  virtual bool post_resume() { return true; }
  // stop
  virtual bool pre_stop( const bool alreadyDone ) { return true; }
  virtual bool on_stop() { return true; }
  virtual bool post_stop() { return true; }
  // clear
  virtual bool pre_clear( const bool alreadyDone ) { return true; }
  virtual bool on_clear() { return true; }
  virtual bool post_clear() { return true; }
  // release
  virtual bool pre_release( const bool alreadyDone ) { return true; }
  virtual bool on_release() { return true; }
  virtual bool post_release() { return true; }

  State      m_State{ State::Type::Empty };
  std::mutex m_mutex;
  enum class Transition : std::int8_t
  {
    allowed     = 1,
    refused     = 0,
    alreadyDone = -1,
  };
  constexpr bool                                                                       shouldExecute( const Transition t ) noexcept { return t == Transition::allowed; }
  constexpr bool                                                                       alreadyDone( const Transition t ) noexcept { return t == Transition::alreadyDone; }
  ReturnValue                                                                          isSuccess( const Transition t ) noexcept { return t != Transition::refused; }
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
    if( it == allowed.end() || !it->second.contains( to ) )
    {
      warn( "{} to {} unauthorised", getStateStr(), State( to ).str() );
      return Transition::refused;
    }
    return Transition::allowed;
  }

  void          send_to_server( const std::string_view str ) { send( str ); }
  std::uint64_t getMaxEvents() const noexcept { return m_max_event.load( std::memory_order_relaxed ); }

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
