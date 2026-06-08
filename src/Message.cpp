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
  m_time = clock::utc_nanoseconds();
  m_uuid = ix::uuid4();
  m_type = Message::Type::Unknown;
}

yaodaq::Message::Message( const yaodaq::Message::Type type ) : Message() { m_type = type; }

yaodaq::Log::Log( const spdlog::details::log_msg& msg ) :
  Message( yaodaq::Message::Type::Log ), m_logger_name( msg.logger_name.data(), msg.logger_name.size() ), m_level( msg.level ), m_message( msg.payload.data(), msg.payload.size() ),
  m_logger_time( std::chrono::duration_cast<std::chrono::nanoseconds>( msg.time.time_since_epoch() ).count() ), m_function_name( msg.source.funcname == nullptr ? "" : std::string( msg.source.funcname ) ),
  m_file_name( msg.source.filename == nullptr ? "" : std::string( msg.source.filename ) ), m_line( msg.source.line )
{
}

yaodaq::Open::Open( const ix::WebSocketOpenInfo& open ) : Message( yaodaq::Message::Type::Open ), m_uri( open.uri ), m_protocol( open.protocol )
{
  for( std::map<std::string, std::string, ix::CaseInsensitiveLess>::const_iterator it = open.headers.cbegin(); it != open.headers.cend(); ++it ) m_headers[it->first] = it->second;
}

yaodaq::Close::Close( const ix::WebSocketCloseInfo& close ) : Message( yaodaq::Message::Type::Close ), m_code( close.code ), m_reason( close.reason ), m_remote( close.remote ) {}

yaodaq::Reject::Reject( const ix::WebSocketCloseInfo& close ) : Message( yaodaq::Message::Type::Reject ), m_code( close.code ), m_reason( close.reason ), m_remote( close.remote ) {}

yaodaq::Error::Error( const ix::WebSocketErrorInfo& error ) :
  Message( yaodaq::Message::Type::Error ), m_retries( error.retries ), m_wait_time( error.wait_time ), m_http_status( error.http_status ), m_reason( error.reason ), m_decompression_error( error.decompressionError )
{
}

yaodaq::Except::Except( const Exception& exception ) : Message( yaodaq::Message::Type::Exception ), m_what( exception.what() ), m_exception_type( "yaodaq::Exception" ) {}

yaodaq::Except::Except( const std::exception& exception ) : Message( yaodaq::Message::Type::Exception ), m_what( exception.what() ), m_exception_type( "std::exception" ) {}

yaodaq::Except::Except( const std::string_view& exception ) : Message( yaodaq::Message::Type::Exception ), m_what( exception ), m_exception_type( "std::string_view" ) {}
