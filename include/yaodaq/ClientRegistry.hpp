#pragma once

#include <ixwebsocket/IXWebSocket.h>
#include <mutex>
#include <unordered_map>
#include <yaodaq/Identifier.hpp>

namespace yaodaq
{

class ClientRegistry
{
public:
  // Add client (returns false if name already exists)
  bool add( const Identifier& id, ix::WebSocket* ws )
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    return m_clients.emplace( id, ws ).second;
  }

  void erase( const Identifier& id )
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    m_clients.erase( id );
  }

  std::size_t size() const
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    return m_clients.size();
  }

  std::unordered_map<Identifier, ix::WebSocket*> get() { return m_clients; }

private:
  mutable std::mutex                             m_mutex;
  std::unordered_map<Identifier, ix::WebSocket*> m_clients;
};

}  // namespace yaodaq
