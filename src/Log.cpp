#include "yaodaq/Log.hpp"

#include "yaodaq/Identifier.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

yaodaq::Log::Log( const Identifier& identifier ) : m_logger( std::make_shared<spdlog::logger>( identifier.id() ) )
{
  {
    std::lock_guard<std::mutex> lock( m_mutex );
    m_logger->sinks().push_back( std::make_shared<spdlog::sinks::stdout_color_sink_mt>() );
  }
}
