#include "yaodaq/Message.hpp"

#include "yaodaq/Version.hpp"

#include <algorithm>
#include <chrono>
#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketOpenInfo.h>
#include <magic_enum/magic_enum.hpp>
#include <spdlog/details/log_msg.h>
#include <string_view>

void yaodaq::Message::setConnectionStateInfos( std::shared_ptr<ix::ConnectionState>& connection )
{
  meta()["remote_ip"]   = connection->getRemoteIp();
  meta()["remote_port"] = connection->getRemotePort();
}

void yaodaq::Message::setWebsocketInfos( ix::WebSocket& socket )
{
  meta()["url"]          = socket.getUrl();
  meta()["sub_protocol"] = socket.getSubProtocols();
}

yaodaq::Message::Message( const yaodaq::Message::Type type )
{
  meta()["yaodaq"]["version"]["major"] = yaodaq::Version::major();
  meta()["yaodaq"]["version"]["minor"] = yaodaq::Version::minor();
  meta()["yaodaq"]["version"]["patch"] = yaodaq::Version::patch();
  meta()["yaodaq"]["version"]["tweak"] = yaodaq::Version::tweak();
  meta()["type"]                       = std::string( magic_enum::enum_name( type ) );
  payload()                            = nlohmann::json::object();
}

yaodaq::Log::Log( const spdlog::details::log_msg& msg ) : Message( yaodaq::Message::Type::Log )
{
  payload()["logger_name"]            = std::string( msg.logger_name.data(), msg.logger_name.size() );
  payload()["level"]                  = static_cast<int>( msg.level );
  payload()["message"]                = std::string( msg.payload.data(), msg.payload.size() );
  payload()["time"]                   = std::chrono::duration_cast<std::chrono::nanoseconds>( msg.time.time_since_epoch() ).count();
  payload()["source_loc"]["filename"] = msg.source.filename == nullptr ? "" : std::string( msg.source.filename );
  payload()["source_loc"]["funcname"] = msg.source.funcname == nullptr ? "" : std::string( msg.source.funcname );
  payload()["source_loc"]["line"]     = msg.source.line;
}

yaodaq::Open::Open( const ix::WebSocketOpenInfo& open ) : Message( yaodaq::Message::Type::Open )
{
  payload()["uri"]      = open.uri;
  payload()["headers"]  = open.headers;
  payload()["protocol"] = open.protocol;
}

yaodaq::Except::Except( const Exception& exception ) : Message( yaodaq::Message::Type::Exception ) { payload()["what"] = exception.what(); }

yaodaq::Except::Except( const std::exception& exception ) : Message( yaodaq::Message::Type::Exception ) { payload()["what"] = exception.what(); }

yaodaq::Except::Except( const std::string_view& exception ) : Message( yaodaq::Message::Type::Exception ) { payload()["what"] = exception; }
