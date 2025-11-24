/**
\copyright Copyright 2022 flagarde
*/

#include "yaodaq/ConnectionState.hpp"

#include "yaodaq/Identifier.hpp"

namespace yaodaq
{

std::list<std::pair<std::string, std::string>> ConnectionState::m_Ids{};

ConnectionState::ConnectionState() : ix::ConnectionState() {}

ConnectionState::~ConnectionState()
{
  std::lock_guard<std::mutex> guard( m_Mutex );
  m_Ids.remove( m_Pair );
}

void ConnectionState::computeId( const std::string& host, const Identifier& id )
{
  std::lock_guard<std::mutex> guard( m_Mutex );
  m_Pair = std::pair<std::string, std::string>( host, id.getName() );

  if( id.empty() ) { _id = std::to_string( _globalId++ ); }
  else
  {
    std::list<std::pair<std::string, std::string>>::iterator found = std::find( m_Ids.begin(), m_Ids.end(), m_Pair );
    if( found == m_Ids.end() )
    {
      _id = id.getName();
      m_Ids.push_back( m_Pair );
    }
    else
    {
      setTerminated();
    }
  }
}

}  // namespace yaodaq
