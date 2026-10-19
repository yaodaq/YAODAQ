#pragma once
#include "yaodaq/Exception.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Message.hpp"

#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class TransactionManager
{
public:
  using MessagePtr = std::unique_ptr<yaodaq::Message>;
  using Promise    = std::promise<MessagePtr>;
  using Future     = std::future<MessagePtr>;
  YAODAQ_API Future create( std::string uuid )
  {
    Promise promise;
    auto    future = promise.get_future();
    {
      std::lock_guard<std::mutex> lock( m_mutex );
      auto [it, inserted] = m_pending.emplace( std::move( uuid ), std::move( promise ) );
      if( !inserted ) throw yaodaq::Exception( "Duplicate transaction UUID" );
    }
    return future;
  }
  // New FIFO mode for protocols without response UUIDs
  YAODAQ_API Future create_fifo()
  {
    Promise promise;
    auto    future = promise.get_future();
    {
      std::lock_guard<std::mutex> lock( m_mutex );
      m_fifo.push_back( std::move( promise ) );
    }

    return future;
  }

  YAODAQ_API std::unique_ptr<yaodaq::Message> resolve( std::unique_ptr<yaodaq::Message> msg )
  {
    if( !msg ) return nullptr;
    Promise promise;
    {
      std::lock_guard<std::mutex> lock( m_mutex );
      // Preserve existing UUID matching first.
      auto                        it = m_pending.find( msg->uuid() );
      if( it != m_pending.end() )
      {
        promise = std::move( it->second );
        m_pending.erase( it );
      }
      else if( !m_fifo.empty() )
      {
        // Associate the response with the oldest FIFO request.
        promise = std::move( m_fifo.front() );
        m_fifo.pop_front();
      }
      else
      {
        return msg;  // Not a recognized response.
      }
    }

    promise.set_value( std::move( msg ) );
    return nullptr;  // consumed by transaction
  }
  YAODAQ_API void reset()
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    m_pending.clear();
    m_fifo.clear();
  }

  YAODAQ_API void shutdown()
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    const auto                  exception = std::make_exception_ptr( yaodaq::Exception( "Transaction manager shutdown" ) );
    for( auto& [uuid, promise]: m_pending )
    {
      try
      {
        promise.set_exception( exception );
      }
      catch( const std::future_error& )
      {
      }
    }
    for( auto& promise: m_fifo )
    {
      try
      {
        promise.set_exception( exception );
      }
      catch( const std::future_error& )
      {
      }
    }
    m_pending.clear();
    m_fifo.clear();
  }

private:
  std::unordered_map<std::string, Promise> m_pending;
  std::deque<Promise>                      m_fifo;
  std::mutex                               m_mutex;
};
