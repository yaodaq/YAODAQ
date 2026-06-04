#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/Message.hpp"

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

class Dispatcher
{
public:
  using Handler = std::function<void( const yaodaq::Message& )>;

  YAODAQ_API void subscribe( yaodaq::Message::Type type, Handler handler )
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    m_handlers[type].push_back( std::move( handler ) );
  }

  YAODAQ_API void subscribeToAll( Handler handler )
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    m_all.push_back( std::move( handler ) );
  }

  YAODAQ_API void dispatch( const yaodaq::Message& message ) const
  {
    std::vector<Handler> handlers;
    std::vector<Handler> all;

    {
      std::lock_guard<std::mutex> lock( m_mutex );

      if( auto it = m_handlers.find( message.type() ); it != m_handlers.end() ) { handlers = it->second; }

      all = m_all;
    }

    for( const auto& handler: handlers ) { handler( message ); }

    for( const auto& handler: all ) { handler( message ); }
  }

private:
  mutable std::mutex m_mutex;

  std::unordered_map<yaodaq::Message::Type, std::vector<Handler>> m_handlers;

  std::vector<Handler> m_all;
};
