#pragma once

#include "Dispatcher.hpp"
#include "ICodec.hpp"
#include "ITransport.hpp"
#include "Logging.hpp"
#include "ThreadSafeQueue.hpp"
#include "TransactionManager.hpp"
#include "yaodaq/Formatter.hpp"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

class Connector
{
public:
  explicit Connector( std::unique_ptr<ITransport> transport, std::unique_ptr<yaodaq::ICodec> codec ) : m_transport( std::move( transport ) ), m_codec( std::move( codec ) ) {}
  ~Connector() noexcept { disconnect(); }

  bool connect()
  {
    if( !m_transport->verifyParameters() )
    {
      if( m_logging ) m_logging->error( "Invalid connector parameters:\n{}", yaodaq::Formatter::format( m_transport->getParameters() ) );
      return false;
    }
    if( m_logging ) m_logging->trace( "Parameters for connector verified:\n{}", yaodaq::Formatter::format( m_transport->getParameters() ) );
    if( !m_transport->open() ) return false;
    try
    {
      m_running      = true;
      m_readerThread = { std::thread( &Connector::readerLoop, this ) };
      m_writerThread = { std::thread( &Connector::writerLoop, this ) };
    }
    catch( const std::exception& e )
    {
      m_running = false;
      if( m_logging ) m_logging->error( "Failed to start threads: {}", e.what() );
      return false;
    }
    catch( ... )
    {
      m_running = false;
      if( m_logging ) m_logging->error( "Failed to start threads" );
      return false;
    }
    return true;
  }

  bool disconnect()
  {
    if( !m_running ) return true;

    m_running = false;

    m_outgoing.shutdown();
    m_transactions.shutdown();
    m_transport->close();

    if( m_readerThread.joinable() ) m_readerThread.join();

    if( m_writerThread.joinable() ) m_writerThread.join();

    m_transactions.shutdown();

    return true;
  }

  void send( const yaodaq::Message& msg ) { m_outgoing.push( msg ); }

  std::future<yaodaq::Message> request( const yaodaq::Message& msg )
  {
    auto future = m_transactions.create( msg.uuid() );
    send( msg );
    return future;
  }

  Dispatcher& dispatcher() { return m_dispatcher; }
  void        setLogger( yaodaq::Logging* logging ) noexcept { m_logging = logging; }

private:
  std::exception_ptr               m_threadException;
  std::mutex                       m_exceptionMutex;
  yaodaq::Logging*                 m_logging{ nullptr };
  std::unique_ptr<ITransport>      m_transport{ nullptr };
  std::unique_ptr<yaodaq::ICodec>  m_codec{ nullptr };
  ThreadSafeQueue<yaodaq::Message> m_outgoing;
  std::thread                      m_readerThread;
  std::thread                      m_writerThread;
  std::atomic<bool>                m_running{ false };
  TransactionManager               m_transactions;
  Dispatcher                       m_dispatcher;

  void writerLoop()
  {
    try
    {
      while( m_running )
      {
        auto msg = m_outgoing.pop();
        if( !msg.has_value() ) break;
        auto raw = m_codec->encode( *msg );
        m_transport->write( raw );
      }
    }
    catch( const std::exception& e )
    {
      if( m_logging ) { m_logging->error( "readerLoop exception: {}", e.what() ); }

      m_running = false;
    }
  }

  void readerLoop()
  {
    try
    {
      while( m_running )
      {
        auto raw = m_transport->read();
        if( !raw.has_value() ) break;
        auto msg = m_codec->decode( *raw );
        if( !m_transactions.resolve( msg ) ) { m_dispatcher.dispatch( msg ); }
      }
    }
    catch( const std::exception& e )
    {
      if( m_logging ) { m_logging->error( "readerLoop exception: {}", e.what() ); }

      m_running = false;
    }
  }
};
