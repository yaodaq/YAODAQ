#pragma once

#include "yaodaq/Message.hpp"

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

class Dispatcher
{
public:
  using Handler      = std::function<void( const yaodaq::Message& )>;
  using ErrorHandler = std::function<void( std::exception_ptr )>;

  template<typename T> void subscribe( std::function<void( const T& t )> handler, ErrorHandler onError = nullptr ) noexcept
  {
    static_assert( std::is_base_of_v<yaodaq::Message, T>, "T must derive from yaodaq::Message" );
    auto wrapper = [h = std::move( handler ), err = std::move( onError )]( const yaodaq::Message& base ) mutable
    {
      try
      {
        h( static_cast<const T&>( base ) );
      }
      catch( ... )
      {
        if( err ) err( std::current_exception() );
        else
          std::cout << "Your dispatcher for type " << yaodaq::Message::type_str( T::type ) << " throw exception !\nAdd a error handler !" << std::endl;
      }
    };
    std::lock_guard<std::mutex> lock( m_mutex );
    m_handlers[T::type].push_back( std::move( wrapper ) );
  }

  void subscribeToAll( Handler handler, ErrorHandler onError = nullptr ) noexcept
  {
    auto wrapper = [h = std::move( handler ), onError = std::move( onError )]( const yaodaq::Message& msg ) mutable
    {
      try
      {
        h( msg );
      }
      catch( ... )
      {
        if( onError ) onError( std::current_exception() );
        else
          std::cout << "Your dispatcher to all throw exception !!! Add a error handler !" << std::endl;
      }
    };
    std::lock_guard<std::mutex> lock( m_mutex );
    m_all.push_back( std::move( wrapper ) );
  }

  void dispatch( const yaodaq::Message& message ) const
  {
    std::vector<Handler> handlers;
    std::vector<Handler> all;
    {
      std::lock_guard<std::mutex> lock( m_mutex );
      if( auto it = m_handlers.find( message.type() ); it != m_handlers.end() ) { handlers = it->second; }

      all = m_all;
    }
    for( const auto& h: handlers ) h( message );
    for( const auto& h: all ) h( message );
  }

private:
  mutable std::mutex                                              m_mutex;
  std::unordered_map<yaodaq::Message::Type, std::vector<Handler>> m_handlers;
  std::vector<Handler>                                            m_all;
};
