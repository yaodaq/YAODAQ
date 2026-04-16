#include "yaodaq/Message.hpp"

#include "yaodaq/Version.hpp"

#include <algorithm>
#include <ixwebsocket/IXWebSocketOpenInfo.h>
#include <magic_enum/magic_enum.hpp>
#include <spdlog/details/log_msg.h>

yaodaq::Message::Message( const yaodaq::Message::Type type )
{
  m_data["yaodaq"]["version"]["major"] = yaodaq::Version::major();
  m_data["yaodaq"]["version"]["minor"] = yaodaq::Version::minor();
  m_data["yaodaq"]["version"]["patch"] = yaodaq::Version::patch();
  m_data["yaodaq"]["version"]["tweak"] = yaodaq::Version::tweak();
  m_data["type"]                       = std::string( magic_enum::enum_name( type ) );
  m_data["content"]                    = nlohmann::json::object();
}

yaodaq::Log::Log( const spdlog::details::log_msg& msg ) : Message( yaodaq::Message::Type::Log )
{
  m_data["content"]["logger_name"]            = std::string( msg.logger_name.data(), msg.logger_name.size() );
  m_data["content"]["level"]                  = static_cast<int>( msg.level );
  m_data["content"]["payload"]                = std::string( msg.payload.data(), msg.payload.size() );
  m_data["content"]["time"]                   = std::chrono::duration_cast<std::chrono::nanoseconds>( msg.time.time_since_epoch() ).count();
  m_data["content"]["source_loc"]["filename"] = msg.source.filename == nullptr ? "" : std::string( msg.source.filename );
  m_data["content"]["source_loc"]["funcname"] = msg.source.funcname == nullptr ? "" : std::string( msg.source.funcname );
  m_data["content"]["source_loc"]["line"]     = msg.source.line;
}

yaodaq::Open::Open( const ix::WebSocketOpenInfo& open ) : Message( yaodaq::Message::Type::Open )
{
  m_data["content"]["uri"]      = open.uri;
  m_data["content"]["headers"]  = open.headers;
  m_data["content"]["protocol"] = open.protocol;
}
