#include "yaodaq/Logging.hpp"

#include "yaodaq/Identifier.hpp"
#include "yaodaq/Message.hpp"

#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

yaodaq::Logging::Logging( const Identifier& identifier ) : m_logger( std::make_shared<spdlog::logger>( identifier.id() ) ), m_logger_websocket( std::make_shared<spdlog::logger>( identifier.id() ) )
{
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    spdlog::set_default_logger( nullptr );
    m_logger->sinks().push_back( std::make_shared<spdlog::sinks::stdout_color_sink_mt>() );
  }
}

void yaodaq::Logging::add_websocket_callback( const std::function<void( const Log& msg )>& f )
{
  m_logger_websocket->set_level( spdlog::level::level_enum::trace );
  auto sink = std::make_shared<spdlog::sinks::callback_sink_mt>( [this, f = std::move( f )]( const spdlog::details::log_msg& msg ) { f( Log( msg ) ); } );
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    m_logger_websocket->sinks().push_back( std::move( sink ) );
  }
}
