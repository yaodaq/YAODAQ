#include "yaodaq/Message.hpp"

#include "yaodaq/Clock.hpp"
#include "yaodaq/Version.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXUuid.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketCloseInfo.h>
#include <ixwebsocket/IXWebSocketErrorInfo.h>
#include <ixwebsocket/IXWebSocketOpenInfo.h>
#include <magic_enum/magic_enum.hpp>
#include <spdlog/details/log_msg.h>
#include <string_view>

std::string_view yaodaq::Message::type_str( const Message::Type type ) noexcept { return magic_enum::enum_name( type ); }

yaodaq::Message::Message() noexcept
{
  m_time                               = clock::utc_nanoseconds();
  m_uuid                               = ix::uuid4();
  m_type                               = Message::Type::Unknown;
  meta()                               = nlohmann::json::object();
  payload()                            = nlohmann::json::object();
  meta()["type"]                       = magic_enum::enum_name( Message::Type::Unknown );
  meta()["yaodaq"]["version"]["major"] = m_version.major();
  meta()["yaodaq"]["version"]["minor"] = m_version.minor();
  meta()["yaodaq"]["version"]["patch"] = m_version.patch();
  meta()["yaodaq"]["version"]["tweak"] = m_version.tweak();
  meta()["uuid"]                       = m_uuid;
  meta()["time"]                       = m_time;
}

yaodaq::Message::Message( const nlohmann::json& json ) : Message()
{
  if( json.contains( "method" ) || json.contains( "notification" ) )
  {
    m_type         = Message::Type::RPCRequest;
    meta()["type"] = magic_enum::enum_name( Message::Type::RPCRequest );
    payload()      = json;
  }
  else if( json.contains( "result" ) || json.contains( "error" ) )
  {
    m_type         = Message::Type::RPCResponse;
    meta()["type"] = magic_enum::enum_name( Message::Type::RPCResponse );
    payload()      = json;
  }
  else if( json.contains( "meta" ) && json["meta"].is_object() && json.contains( "payload" ) && json["payload"].is_object() )
  {
    meta()    = json["meta"];
    payload() = json["payload"];
    m_type    = magic_enum::enum_cast<yaodaq::Message::Type>( json["meta"]["type"].get<std::string_view>(), magic_enum::case_insensitive ).value_or( Type::Unknown );
  }
  else
  {
    meta()["type"] = magic_enum::enum_name( Message::Type::Unknown );
    m_type         = Message::Type::Unknown;
    payload()      = json;
  }
}

yaodaq::Message::Message( const yaodaq::Message::Type type ) : Message()
{
  meta()["type"] = std::string( magic_enum::enum_name( type ) );
  m_type         = type;
}

yaodaq::Log::Log( const spdlog::details::log_msg& msg ) :
  Message( yaodaq::Message::Type::Log ), m_logger_name( msg.logger_name.data(), msg.logger_name.size() ), m_level( msg.level ), m_message( msg.payload.data(), msg.payload.size() ),
  m_logger_time( std::chrono::duration_cast<std::chrono::nanoseconds>( msg.time.time_since_epoch() ).count() ), m_function_name( msg.source.funcname == nullptr ? "" : std::string( msg.source.funcname ) ),
  m_file_name( msg.source.filename == nullptr ? "" : std::string( msg.source.filename ) ), m_line( msg.source.line )
{
  payload()["logger_name"]            = std::string( msg.logger_name.data(), msg.logger_name.size() );
  payload()["level"]                  = static_cast<int>( msg.level );
  payload()["message"]                = std::string( msg.payload.data(), msg.payload.size() );
  payload()["time"]                   = std::chrono::duration_cast<std::chrono::nanoseconds>( msg.time.time_since_epoch() ).count();
  payload()["source_loc"]["filename"] = msg.source.filename == nullptr ? "" : std::string( msg.source.filename );
  payload()["source_loc"]["funcname"] = msg.source.funcname == nullptr ? "" : std::string( msg.source.funcname );
  payload()["source_loc"]["line"]     = msg.source.line;
}

yaodaq::Open::Open( const ix::WebSocketOpenInfo& open ) : Message( yaodaq::Message::Type::Open ), m_uri( open.uri ), m_protocol( open.protocol )
{
  payload()["uri"]      = open.uri;
  payload()["headers"]  = open.headers;
  payload()["protocol"] = open.protocol;
  for( std::map<std::string, std::string, ix::CaseInsensitiveLess>::const_iterator it = open.headers.cbegin(); it != open.headers.cend(); ++it ) m_headers[it->first] = it->second;
}

yaodaq::Close::Close( const ix::WebSocketCloseInfo& close ) : Message( yaodaq::Message::Type::Close ), m_code( close.code ), m_reason( close.reason ), m_remote( close.remote )
{
  payload()["code"]   = close.code;
  payload()["reason"] = close.reason;
  payload()["remote"] = close.remote;
}

yaodaq::Reject::Reject( const ix::WebSocketCloseInfo& close ) : Message( yaodaq::Message::Type::Reject ), m_code( close.code ), m_reason( close.reason ), m_remote( close.remote )
{
  payload()["code"]   = close.code;
  payload()["reason"] = close.reason;
  payload()["remote"] = close.remote;
}

yaodaq::Error::Error( const ix::WebSocketErrorInfo& error ) :
  Message( yaodaq::Message::Type::Error ), m_retries( error.retries ), m_wait_time( error.wait_time ), m_http_status( error.http_status ), m_reason( error.reason ), m_decompression_error( error.decompressionError )
{
  payload()["retries"]             = error.retries;
  payload()["wait_time"]           = error.wait_time;
  payload()["http_status"]         = error.http_status;
  payload()["reason"]              = error.reason;
  payload()["decompression_error"] = error.decompressionError;
}

yaodaq::Except::Except( const Exception& exception ) : Message( yaodaq::Message::Type::Exception ), m_what( exception.what() ) { payload()["what"] = exception.what(); }

yaodaq::Except::Except( const std::exception& exception ) : Message( yaodaq::Message::Type::Exception ), m_what( exception.what() ) { payload()["what"] = exception.what(); }

yaodaq::Except::Except( const std::string_view& exception ) : Message( yaodaq::Message::Type::Exception ), m_what( exception ) { payload()["what"] = exception; }
