#pragma once
#include "Dispatcher.hpp"
#include "ICodec.hpp"
#include "ITransport.hpp"
#include "Logging.hpp"
#include "ThreadSafeQueue.hpp"
#include "TransactionManager.hpp"
#include "yaodaq/Export.hpp"
#include "yaodaq/Formatter.hpp"

#include <atomic>
#include <memory>
#include <thread>

class Connector
{
public:
  YAODAQ_API Connector( std::unique_ptr<ITransport> transport, std::unique_ptr<yaodaq::ICodec> codec ) : m_transport( std::move( transport ) ), m_codec( std::move( codec ) ) {}

  YAODAQ_API ~Connector() noexcept { disconnect(); }

  YAODAQ_API bool connect()
  {
    if( !m_transport->verifyParameters() )
    {
      if( m_logging ) { m_logging->error( "Invalid connector parameters:\n{}", yaodaq::Formatter::format( m_transport->getParameters() ).data() ); }
      return false;
    }

    if( m_logging ) { m_logging->trace( "Parameters for connector verified:\n{}", yaodaq::Formatter::format( m_transport->getParameters() ).data() ); }

    if( !m_transport->open() ) return false;

    try
    {
      m_running = true;

      m_readerThread = std::thread( &Connector::readerLoop, this );
      m_writerThread = std::thread( &Connector::writerLoop, this );
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

  YAODAQ_API bool disconnect()
  {
    if( !m_running ) return true;

    m_running = false;

    m_outgoing.shutdown();
    m_transactions.shutdown();

    m_transport->close();

    if( m_readerThread.joinable() ) m_readerThread.join();

    if( m_writerThread.joinable() ) m_writerThread.join();

    return true;
  }

  YAODAQ_API void send( std::unique_ptr<yaodaq::Message> msg ) { m_outgoing.push( std::move( msg ) ); }

  YAODAQ_API std::future<std::unique_ptr<yaodaq::Message>> request( std::unique_ptr<yaodaq::Message> msg )
  {
    auto future = m_transactions.create( msg->uuid() );
    send( std::move( msg ) );
    return future;
  }

  YAODAQ_API Dispatcher& dispatcher() { return m_dispatcher; }

  YAODAQ_API void setLogger( yaodaq::Logging* logging ) noexcept { m_logging = logging; }

private:
  yaodaq::Logging* m_logging{ nullptr };

  std::unique_ptr<ITransport>     m_transport;
  std::unique_ptr<yaodaq::ICodec> m_codec;

  ThreadSafeQueue<std::unique_ptr<yaodaq::Message>> m_outgoing;

  std::thread m_readerThread;
  std::thread m_writerThread;

  std::atomic<bool> m_running{ false };

  TransactionManager m_transactions;
  Dispatcher         m_dispatcher;

private:
  void writerLoop()
  {
    try
    {
      while( m_running )
      {
        auto msgOpt = m_outgoing.pop();
        if( !msgOpt.has_value() ) break;

        auto& msg = msgOpt.value();
        auto  raw = m_codec->encode( *msg );
        m_transport->write( raw );
      }
    }
    catch( const std::exception& e )
    {
      if( m_logging ) m_logging->error( "writerLoop exception: {}", e.what() );

      m_running = false;
    }
  }

  void readerLoop()
  {
    try
    {
      while( m_running )
      {
        std::optional<std::vector<std::byte>> raw;
        try
        {
          raw = m_transport->read();
          if( !raw.has_value() ) break;
        }
        catch( ... )
        {
          std::cout << "OUPSSSSSSSSSSSSS " << std::endl;
          continue;
        }
        std::unique_ptr<yaodaq::Message> msg;
        try
        {
          msg = m_codec->decode( *raw );
        }
        catch( const nlohmann::json::parse_error& e )
        {
          m_logging->warn( "JSON parse error: {}", e.what() );
          continue;
        }
        catch( const std::exception& e )
        {
          m_logging->error( "decode failed: {}", e.what() );
          continue;
        }

        // TransactionManager consumes or returns ownership

        try
        {
          if( msg ) msg = m_transactions.resolve( std::move( msg ) );
        }
        catch( ... )
        {
          std::cout << "OUPSSSS transaction" << std::endl;
          continue;
        }
        try
        {
          if( msg ) { m_dispatcher.dispatch( *msg.get() ); }
        }
        catch( ... )
        {
          std::cout << "OUPPP dispatch" << std::endl;
          continue;
        }
      }
    }
    catch( const std::exception& e )
    {
      if( m_logging ) m_logging->error( "readerLoop exception: {}", e.what() );

      m_running = false;
    }
  }
};
