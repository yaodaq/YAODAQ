#pragma once
#include "yaodaq/Exception.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Message.hpp"

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

  YAODAQ_API std::unique_ptr<yaodaq::Message> resolve( std::unique_ptr<yaodaq::Message> msg )
  {
    if( !msg ) return nullptr;

    std::promise<std::unique_ptr<yaodaq::Message>> promise;

    {
      std::lock_guard<std::mutex> lock( m_mutex );

      auto it = m_pending.find( msg->uuid() );

      if( it == m_pending.end() ) return msg;  // not a response → forward it

      promise = std::move( it->second );
      m_pending.erase( it );
    }

    promise.set_value( std::move( msg ) );
    return nullptr;  // consumed by transaction
  }

  YAODAQ_API void shutdown()
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    for( auto& [uuid, promise]: m_pending )
    {
      try
      {
        promise.set_exception( std::make_exception_ptr( yaodaq::Exception( "Transaction manager shutdown" ) ) );
      }
      catch( const std::future_error& )
      {
      }
    }
    m_pending.clear();
  }

private:
  std::unordered_map<std::string, Promise> m_pending;
  std::mutex                               m_mutex;
};
