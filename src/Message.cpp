#include "yaodaq/Message.hpp"

#include "yaodaq/Version.hpp"

#include <algorithm>
#include <chrono>
#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketCloseInfo.h>
#include <ixwebsocket/IXWebSocketErrorInfo.h>
#include <ixwebsocket/IXWebSocketOpenInfo.h>
#include <magic_enum/magic_enum.hpp>
#include <spdlog/details/log_msg.h>
#include <string_view>

yaodaq::Message::Message() noexcept
{
  meta()                               = nlohmann::json::object();
  payload()                            = nlohmann::json::object();
  meta()["type"]                       = magic_enum::enum_name( Message::Type::Unknown );
  meta()["yaodaq"]["version"]["major"] = yaodaq::Version::major();
  meta()["yaodaq"]["version"]["minor"] = yaodaq::Version::minor();
  meta()["yaodaq"]["version"]["patch"] = yaodaq::Version::patch();
  meta()["yaodaq"]["version"]["tweak"] = yaodaq::Version::tweak();
}

yaodaq::Message::Type yaodaq::Message::type() const noexcept { return magic_enum::enum_cast<yaodaq::Message::Type>( meta()["type"].get<std::string_view>(), magic_enum::case_insensitive ).value(); }

yaodaq::Message::Message( const nlohmann::json& json ) : Message()
{
  if( json.contains( "method" ) || json.contains( "notification" ) )
  {
    meta()["type"] = magic_enum::enum_name( Message::Type::RPCRequest );
    payload()      = json;
  }
  else if( json.contains( "result" ) || json.contains( "error" ) )
  {
    meta()["type"] = magic_enum::enum_name( Message::Type::RPCResponse );
    payload()      = json;
  }
  else if( json.contains( "meta" ) && json["meta"].is_object() && json.contains( "payload" ) && json["payload"].is_object() )
  {
    meta()    = json["meta"];
    payload() = json["payload"];
  }
}

yaodaq::Message::Message( const yaodaq::Message::Type type ) : Message() { meta()["type"] = std::string( magic_enum::enum_name( type ) ); }

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

yaodaq::Close::Close( const ix::WebSocketCloseInfo& close ) : Message( yaodaq::Message::Type::Close )
{
  payload()["code"]   = close.code;
  payload()["reason"] = close.reason;
  payload()["remote"] = close.remote;
}

yaodaq::Reject::Reject( const ix::WebSocketCloseInfo& close ) : Message( yaodaq::Message::Type::Reject )
{
  payload()["code"]   = close.code;
  payload()["reason"] = close.reason;
  payload()["remote"] = close.remote;
}

yaodaq::Error::Error( const ix::WebSocketErrorInfo& error ) : Message( yaodaq::Message::Type::Error )
{
  payload()["retries"]             = error.retries;
  payload()["wait_time"]           = error.wait_time;
  payload()["http_status"]         = error.http_status;
  payload()["reason"]              = error.reason;
  payload()["decompression_error"] = error.decompressionError;
}

yaodaq::Except::Except( const Exception& exception ) : Message( yaodaq::Message::Type::Exception ) { payload()["what"] = exception.what(); }

yaodaq::Except::Except( const std::exception& exception ) : Message( yaodaq::Message::Type::Exception ) { payload()["what"] = exception.what(); }

yaodaq::Except::Except( const std::string_view& exception ) : Message( yaodaq::Message::Type::Exception ) { payload()["what"] = exception; }
