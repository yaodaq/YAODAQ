#pragma once
#include "State.hpp"
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
  YAODAQ_API Module( const std::string_view name, const std::string_view host = defaults::host, const std::uint16_t port = defaults::port, const std::string_view path = defaults::path, const std::string_view type = "UserType" ) :
    Client( { Component::role::Module, type, name }, host, port, path )
  {
    Add( "initialize", GetHandle( &yaodaq::Module::initialize, *this ) );
    Add( "configure", GetHandle( &yaodaq::Module::configure, *this ) );
    Add( "start", GetHandle( &yaodaq::Module::start, *this ) );
    Add( "stop", GetHandle( &yaodaq::Module::stop, *this ) );
    Add( "pause", GetHandle( &yaodaq::Module::pause, *this ) );
    Add( "resume", GetHandle( &yaodaq::Module::resume, *this ) );
    Add( "clear", GetHandle( &yaodaq::Module::clear, *this ) );
    Add( "release", GetHandle( &yaodaq::Module::release, *this ) );
    Add( "relink", GetHandle( &yaodaq::Module::relink, *this ) );
    Add( "getState", GetHandle( &yaodaq::Module::getStateStr, *this ) );
  }

  // Events
  YAODAQ_API bool link()
  {
    yaodaq::Client::start();
    m_State.setId( State::ID::Linked );
    return true;
  }

  YAODAQ_API bool initialize()
  {
    logger()->info( "Initializing" );
    bool ret = on_initialize();
    m_State.setId( State::ID::Initialized );
    return ret;
  }

  YAODAQ_API bool configure()
  {
    logger()->info( "Configuring" );
    bool ret = on_configure();
    m_State.setId( State::ID::Configured );
    return ret;
  }

  YAODAQ_API bool start()
  {
    logger()->info( "Starting" );

    // Check if the worker thread is already running
    if( m_worker.joinable() )
    {
      logger()->warn( "Worker thread is already running." );
      return true;
    }

    // Reset state variables
    m_stopped.store( false );
    m_paused.store( false );
    m_event_nbr.store( 0 );

    // Call on_start() before launching the thread
    bool start_success = on_start();
    if( !start_success )
    {
      logger()->error( "on_start() failed." );
      return false;
    }
    m_worker = std::thread(
      [this]()
      {
        while( !m_stopped.load() )
        {
          std::unique_lock lk( m_mutex );
          cv.wait( lk, [this]() { return !m_paused.load() || m_stopped.load(); } );

          if( m_stopped.load() )
          {
            logger()->info( "Worker thread stopped." );
            break;
          }

          // Release the lock while executing run() to allow pause/resume
          lk.unlock();
          try
          {
            if( !run() )
            {
              logger()->error( "Run failed, stopping worker thread." );
              m_stopped.store( true );
              break;
            }
            else
            {
              m_event_nbr++;
            }
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
          lk.lock();  // Reacquire the lock for the next iteration
        }
      } );

    // Update state
    {
      std::unique_lock lk( m_mutex );
      m_State.setId( State::ID::Started );
    }
    logger()->info( "I'm started" );
    return true;
  }

  YAODAQ_API bool pause()
  {
    logger()->info( "Pausing" );

    {
      std::unique_lock lk( m_mutex );
      m_paused.store( true );
    }
    bool ret = on_pause();
    cv.notify_all();  // Notify the worker thread to check the pause state
    {
      std::unique_lock lk( m_mutex );
      m_State.setId( State::ID::Paused );
    }
    return ret;
  }

  YAODAQ_API bool resume()
  {
    logger()->info( "Resuming" );
    {
      std::unique_lock lk( m_mutex );
      m_paused.store( false );
    }
    bool ret = on_resume();
    cv.notify_all();
    {
      std::unique_lock lk( m_mutex );
      m_State.setId( State::ID::Started );
    }
    return ret;
  }

  YAODAQ_API bool stop()
  {
    logger()->info( "Stopping" );
    {
      std::unique_lock lk( m_mutex );
      m_stopped.store( true );
      m_paused.store( false );
      m_event_nbr.store( 0 );
    }
    bool ret = on_stop();
    cv.notify_all();
    if( m_worker.joinable() ) { m_worker.join(); }
    m_State.setId( State::ID::Stopped );
    return ret;
  }

  YAODAQ_API bool clear()
  {
    logger()->info( "Clearing" );
    bool ret = on_clear();
    m_State.setId( State::ID::Cleared );
    return ret;
  }

  YAODAQ_API bool release()
  {
    logger()->info( "Releasing" );
    bool ret = on_release();
    m_State.setId( State::ID::Released );
    return ret;
  }

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

  YAODAQ_API explicit Module() noexcept = delete;
  YAODAQ_API virtual ~Module() noexcept
  {
    if( m_worker.joinable() )
    {
      {
        std::unique_lock lk( m_mutex );
        m_stopped.store( true );
      }
      cv.notify_all();
      m_worker.join();
    }
  }

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
  virtual bool run()
  {
    logger()->info( "running... event {}", m_event_nbr.load() );
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
    return true;
  }
  State m_State;

private:
  std::atomic<bool>          m_paused{ false };
  std::atomic<bool>          m_stopped{ false };
  std::mutex                 m_mutex;
  std::condition_variable    cv;
  std::thread                m_worker;
  std::atomic<std::uint64_t> m_event_nbr{ 0 };
};

}  // namespace yaodaq
