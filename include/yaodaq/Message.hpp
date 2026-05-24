#pragma once
#include "yaodaq/Exception.hpp"
#include "yaodaq/Export.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <map>
#include <new>
#include <nlohmann/json.hpp>

namespace spdlog
{
namespace details
{
struct log_msg;
}
}  // namespace spdlog

namespace ix
{
struct WebSocketOpenInfo;
struct WebSocketCloseInfo;
struct WebSocketErrorInfo;
class ConnectionState;
class WebSocket;
}  // namespace ix

namespace yaodaq
{

class Message
{
public:
  enum class Type : std::uint8_t
  {
    Log,
    Open,
    Close,
    Reject,
    Error,
    Ping,
    Pong,
    Exception,
    Unknown,
    RPCRequest,
    RPCResponse,
    RawData,
    Raw,
  };
  YAODAQ_API explicit Message( const nlohmann::json& json );
  YAODAQ_API std::string dump( const std::size_t i = 0 ) const { return m_data.dump( i ); }
  YAODAQ_API const nlohmann::json& payload() const noexcept { return m_data["payload"]; }
  YAODAQ_API const nlohmann::json& meta() const noexcept { return m_data["meta"]; }
  YAODAQ_API const nlohmann::json& operator()() const noexcept { return m_data; }
  YAODAQ_API nlohmann::json& payload() noexcept { return m_data["payload"]; }
  YAODAQ_API nlohmann::json& meta() noexcept { return m_data["meta"]; }
  YAODAQ_API Type            type() const noexcept;
  YAODAQ_API std::string uuid() const noexcept { return meta()["uuid"]; }
  YAODAQ_API std::int64_t time() const noexcept { return meta()["time"]; }

protected:
  YAODAQ_API explicit Message( const Type type );
  nlohmann::json m_data;

private:
  YAODAQ_API explicit Message() noexcept;
};

class Log : public Message
{
public:
  YAODAQ_API explicit Log() noexcept = delete;
  YAODAQ_API explicit Log( const spdlog::details::log_msg& msg );
};

class Open : public Message
{
public:
  YAODAQ_API explicit Open() noexcept = delete;
  YAODAQ_API explicit Open( const ix::WebSocketOpenInfo& open );
  YAODAQ_API std::string uri() const noexcept { return payload()["uri"].get<std::string>(); }
  YAODAQ_API std::map<std::string, std::string> headers() const noexcept { return payload()["headers"].get<std::map<std::string, std::string>>(); }
  YAODAQ_API std::string protocol() const noexcept { return payload()["protocol"].get<std::string>(); }
};

class Close : public Message
{
public:
  YAODAQ_API explicit Close() noexcept = delete;
  YAODAQ_API explicit Close( const ix::WebSocketCloseInfo& close );
  YAODAQ_API std::uint16_t code() const noexcept { return payload()["code"].get<std::uint16_t>(); }
  YAODAQ_API std::string_view reason() const noexcept { return payload()["reason"].get<std::string_view>(); }
  YAODAQ_API bool             remote() const noexcept { return payload()["remote"].get<bool>(); }
};

class Reject : public Message
{
public:
  YAODAQ_API explicit Reject() noexcept = delete;
  YAODAQ_API explicit Reject( const ix::WebSocketCloseInfo& close );
  YAODAQ_API std::uint16_t code() const noexcept { return payload()["code"].get<std::uint16_t>(); }
  YAODAQ_API std::string_view reason() const noexcept { return payload()["reason"].get<std::string_view>(); }
  YAODAQ_API bool             remote() const noexcept { return payload()["remote"].get<bool>(); }
};

class Error : public Message
{
public:
  YAODAQ_API explicit Error() noexcept = delete;
  YAODAQ_API explicit Error( const ix::WebSocketErrorInfo& error );
  YAODAQ_API std::uint32_t retries() const noexcept { return payload()["retries"].get<std::uint32_t>(); }
  YAODAQ_API double        wait_time() const noexcept { return payload()["wait_time"].get<double>(); }
  YAODAQ_API int           http_status() const noexcept { return payload()["http_status"].get<int>(); }
  YAODAQ_API std::string_view reason() const noexcept { return payload()["reason"].get<std::string_view>(); }
  YAODAQ_API bool             decompression_error() const noexcept { return payload()["decompression_error"].get<bool>(); }
};

class Ping : public Message
{
public:
  YAODAQ_API explicit Ping() noexcept = delete;
  YAODAQ_API explicit Ping( const std::string_view& message, const std::size_t size, const bool binary ) : Message( yaodaq::Message::Type::Ping )
  {
    payload()["message"] = message;
    payload()["binary"]  = binary;
    payload()["size"]    = size;
  }
  YAODAQ_API std::string_view message() const noexcept { return payload()["message"].get<std::string_view>(); }
  YAODAQ_API bool             binary() const noexcept { return payload()["binary"].get<bool>(); }
  YAODAQ_API std::size_t size() const noexcept { return payload()["size"].get<std::size_t>(); }
};

class Pong : public Message
{
public:
  YAODAQ_API explicit Pong() noexcept = delete;
  YAODAQ_API explicit Pong( const std::string_view& message, const std::size_t size, const bool binary ) : Message( yaodaq::Message::Type::Pong )
  {
    payload()["message"] = message;
    payload()["binary"]  = binary;
    payload()["size"]    = size;
  }
  YAODAQ_API std::string_view message() const noexcept { return payload()["message"].get<std::string_view>(); }
  YAODAQ_API bool             binary() const noexcept { return payload()["binary"].get<bool>(); }
  YAODAQ_API std::size_t size() const noexcept { return payload()["size"].get<std::size_t>(); }
};

class Except : public Message
{
public:
  YAODAQ_API explicit Except() noexcept = delete;
  YAODAQ_API explicit Except( const Exception& exception );
  YAODAQ_API explicit Except( const std::exception& exception );
  YAODAQ_API explicit Except( const std::string_view& exception );
  YAODAQ_API std::string_view what() const noexcept { return payload()["what"].get<std::string_view>(); }
};

class RawData : public Message
{
public:
  YAODAQ_API explicit RawData() : Message( Message::Type::RawData ) {}
  YAODAQ_API explicit RawData( const std::string_view message ) : Message( Message::Type::RawData ) { payload()["message"] = std::move( message ); }
};

class Raw : public Message
{
public:
  YAODAQ_API explicit Raw() : Message( Message::Type::Raw ) {}
  YAODAQ_API explicit Raw( nlohmann::json json ) : Message( Message::Type::Raw ) { m_data = std::move( json ); }
};

}  // namespace yaodaq
