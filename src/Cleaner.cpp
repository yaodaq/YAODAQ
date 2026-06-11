#include "yaodaq/Cleaner.hpp"

#include "yaodaq/Client.hpp"
#include "yaodaq/Server.hpp"
bool yaodaq::Cleaner::add( Client* c )
{
  std::lock_guard lock( m_mutex );

  auto it = std::find( m_clients.begin(), m_clients.end(), c );
  if( it != m_clients.end() )
  {
    std::cout << "replace" << std::endl;
    // Already exists, replace with new pointer (same in this case)
    *it = c;
    return true;  // consider returning false if you want to signal "replaced"
  }
  else
  {
    m_clients.push_back( c );
    return true;
  }
}

bool yaodaq::Cleaner::add( Server* s )
{
  std::lock_guard lock( m_mutex );

  auto it = std::find( m_server.begin(), m_server.end(), s );
  if( it != m_server.end() )
  {
    *it = s;  // replace existing pointer
    return true;
  }
  else
  {
    m_server.push_back( s );
    return true;
  }
}
void yaodaq::Cleaner::remove( Client* c )
{
  std::lock_guard lock( m_mutex );
  auto            it = std::find( m_clients.begin(), m_clients.end(), c );
  if( it != m_clients.end() )
  {
    c->cleanup();           // cleanup first
    m_clients.erase( it );  // then remove from vector
  }
}

void yaodaq::Cleaner::remove( Server* s )
{
  std::lock_guard lock( m_mutex );

  auto it = std::find( m_server.begin(), m_server.end(), s );
  if( it != m_server.end() )
  {
    s->cleanup();          // cleanup first
    m_server.erase( it );  // then remove from vector
  }
}
void yaodaq::Cleaner::clean()
{
  std::cout << "cleaning" << std::endl;
  std::lock_guard lock( m_mutex );
  for( auto* c: m_clients )
  {
    std::cout << "cleaning" << std::endl;
    if( c ) c->cleanup();
  }
  //m_clients.clear();
}
