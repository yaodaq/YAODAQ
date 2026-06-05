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
#include <span>
#include <string_view>

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
    Data,
  };
  virtual ~Message() = default;
  YAODAQ_API explicit Message( const nlohmann::json& json );
  YAODAQ_API std::string dump( const std::size_t i = 0 ) const { return m_data.dump( i ); }
  YAODAQ_API const nlohmann::json& payload() const noexcept { return m_data["payload"]; }
  YAODAQ_API const nlohmann::json& meta() const noexcept { return m_data["meta"]; }
  YAODAQ_API const nlohmann::json& operator()() const noexcept { return m_data; }
  YAODAQ_API nlohmann::json& payload() noexcept { return m_data["payload"]; }
  YAODAQ_API nlohmann::json& meta() noexcept { return m_data["meta"]; }
  YAODAQ_API Type            type() const noexcept;
  YAODAQ_API std::string_view type_str() const noexcept { return meta()["type"].get<std::string_view>(); };
  YAODAQ_API std::string uuid() const noexcept { return meta()["uuid"]; }
  YAODAQ_API std::int64_t            time() const noexcept { return meta()["time"]; }
  YAODAQ_API static std::string_view type_str( const Message::Type t ) noexcept;

protected:
  YAODAQ_API explicit Message( const Type type );
  nlohmann::json m_data;

private:
  YAODAQ_API explicit Message() noexcept;
};

class Log : public Message
{
public:
  static constexpr Message::Type type = Message::Type::Log;
  YAODAQ_API explicit Log() noexcept  = delete;
  YAODAQ_API explicit Log( const spdlog::details::log_msg& msg );
};

class Open : public Message
{
public:
  static constexpr Message::Type type = Message::Type::Open;
  YAODAQ_API explicit Open() noexcept = delete;
  YAODAQ_API explicit Open( const ix::WebSocketOpenInfo& open );
  YAODAQ_API std::string uri() const noexcept { return payload()["uri"].get<std::string>(); }
  YAODAQ_API std::map<std::string, std::string> headers() const noexcept { return payload()["headers"].get<std::map<std::string, std::string>>(); }
  YAODAQ_API std::string protocol() const noexcept { return payload()["protocol"].get<std::string>(); }

private:
  std::string                        m_uri;
  std::map<std::string, std::string> m_headers;
  std::string                        m_protocol;
};

class Close : public Message
{
public:
  static constexpr Message::Type type  = Message::Type::Close;
  YAODAQ_API explicit Close() noexcept = delete;
  YAODAQ_API explicit Close( const ix::WebSocketCloseInfo& close );
  YAODAQ_API std::uint16_t code() const noexcept { return payload()["code"].get<std::uint16_t>(); }
  YAODAQ_API std::string reason() const noexcept { return payload()["reason"].get<std::string>(); }
  YAODAQ_API bool        remote() const noexcept { return payload()["remote"].get<bool>(); }

private:
  std::string   m_reason;
  std::uint16_t m_code{ 0 };
  bool          m_remote{ false };
};

class Reject : public Message
{
public:
  static constexpr Message::Type type   = Message::Type::Reject;
  YAODAQ_API explicit Reject() noexcept = delete;
  YAODAQ_API explicit Reject( const ix::WebSocketCloseInfo& close );
  YAODAQ_API std::uint16_t code() const noexcept { return payload()["code"].get<std::uint16_t>(); }
  YAODAQ_API std::string reason() const noexcept { return payload()["reason"].get<std::string>(); }
  YAODAQ_API bool        remote() const noexcept { return payload()["remote"].get<bool>(); }

private:
  std::string   m_reason;
  std::uint16_t m_code{ 0 };
  bool          m_remote{ false };
};

class Error : public Message
{
public:
  static constexpr Message::Type type  = Message::Type::Error;
  YAODAQ_API explicit Error() noexcept = delete;
  YAODAQ_API explicit Error( const ix::WebSocketErrorInfo& error );
  YAODAQ_API std::uint32_t retries() const noexcept { return payload()["retries"].get<std::uint32_t>(); }
  YAODAQ_API double        wait_time() const noexcept { return payload()["wait_time"].get<double>(); }
  YAODAQ_API int           http_status() const noexcept { return payload()["http_status"].get<int>(); }
  YAODAQ_API std::string reason() const noexcept { return payload()["reason"].get<std::string>(); }
  YAODAQ_API bool        decompression_error() const noexcept { return payload()["decompression_error"].get<bool>(); }

private:
  std::string   m_reason;
  std::uint32_t m_retries{ 0 };
  double        m_wait_time{ .0 };
  int           m_http_status{ 0 };
  bool          m_decompression_error{ false };
};

class Ping : public Message
{
public:
  static constexpr Message::Type type = Message::Type::Ping;
  YAODAQ_API explicit Ping() noexcept = delete;
  YAODAQ_API explicit Ping( const std::string_view& message, const std::size_t size, const bool binary ) : Message( yaodaq::Message::Type::Ping )
  {
    payload()["message"] = message;
    payload()["binary"]  = binary;
    payload()["size"]    = size;
  }
  YAODAQ_API std::string message() const noexcept { return payload()["message"].get<std::string>(); }
  YAODAQ_API bool        binary() const noexcept { return payload()["binary"].get<bool>(); }
  YAODAQ_API std::size_t size() const noexcept { return payload()["size"].get<std::size_t>(); }

private:
  std::string m_message;
  std::size_t m_size{ 0 };
  bool        m_binary{ false };
};

class Pong : public Message
{
public:
  static constexpr Message::Type type = Message::Type::Pong;
  YAODAQ_API explicit Pong() noexcept = delete;
  YAODAQ_API explicit Pong( const std::string_view& message, const std::size_t size, const bool binary ) : Message( yaodaq::Message::Type::Pong )
  {
    payload()["message"] = message;
    payload()["binary"]  = binary;
    payload()["size"]    = size;
  }
  YAODAQ_API std::string message() const noexcept { return payload()["message"].get<std::string>(); }
  YAODAQ_API bool        binary() const noexcept { return payload()["binary"].get<bool>(); }
  YAODAQ_API std::size_t size() const noexcept { return payload()["size"].get<std::size_t>(); }

private:
  std::string m_message;
  std::size_t m_size{ 0 };
  bool        m_binary{ false };
};

class Except : public Message
{
public:
  static constexpr Message::Type type   = Message::Type::Exception;
  YAODAQ_API explicit Except() noexcept = delete;
  YAODAQ_API explicit Except( const Exception& exception );
  YAODAQ_API explicit Except( const std::exception& exception );
  YAODAQ_API explicit Except( const std::string_view& exception );
  YAODAQ_API std::string what() const noexcept { return payload()["what"].get<std::string>(); }

private:
  std::string m_what;
  std::string m_exception_type;
};

/*
struct MessageMeta
{
  std::string codec;     // "json", "protobuf", "raw"
  std::string version;   // "1", "2", "2026-01"
  std::string schema;    // optional schema id
};

struct MessageMeta
{
  // identity / routing
  std::string uuid;          // correlation / request-response
  std::uint64_t timestamp;   // optional ordering

  // transport + decoding
  std::string codec;         // "json", "protobuf", "raw", "msgpack"
  std::uint16_t version;     // protocol version (NOT schema version)

  // routing (optional but very useful)
  std::string topic;         // pub/sub routing key
  std::string reply_to;      // RPC-style responses

  // system flags
  bool compressed = false;
  bool encrypted  = false;
};
*/

class RawData : public Message
{
public:
  static constexpr Message::Type type = Message::Type::RawData;
  explicit RawData( std::string_view topic ) : Message( Message::Type::RawData ), m_topic( topic ) {}
  explicit RawData( std::span<const std::byte> raw_data, std::string_view topic ) : Message( Message::Type::RawData ), m_topic( topic ), m_payload( raw_data.begin(), raw_data.end() ) {}
  std::string_view           topic() const noexcept { return m_topic; }
  std::span<const std::byte> raw() const noexcept { return m_payload; }

private:
  std::string            m_topic;
  std::vector<std::byte> m_payload;
};

class RawDataBuilder
{
public:
  static RawData from_text( const std::string_view text, const std::string_view topic ) { return RawData( std::span<const std::byte>( reinterpret_cast<const std::byte*>( text.data() ), text.size() ), topic ); }
};

}  // namespace yaodaq
