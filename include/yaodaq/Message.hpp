#pragma once
#include "yaodaq/Exception.hpp"
#include "yaodaq/Export.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
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
    Exception
  };
  YAODAQ_API explicit Message() noexcept = delete;
  YAODAQ_API explicit Message( const Type type );
  YAODAQ_API std::string dump( const std::size_t i = 0 ) const { return m_data.dump( i ); }
  YAODAQ_API void        setConnectionStateInfos( std::shared_ptr<ix::ConnectionState>& connection );
  YAODAQ_API void        setWebsocketInfos( ix::WebSocket& socket );
  YAODAQ_API const nlohmann::json& payload() const noexcept { return m_data["payload"]; }
  YAODAQ_API const nlohmann::json& meta() const noexcept { return m_data["meta"]; }
  YAODAQ_API const nlohmann::json& operator()() const noexcept { return m_data; }
  YAODAQ_API std::string url() const noexcept { return meta()["url"]; }
  YAODAQ_API std::string subprotocol() const noexcept { return meta()["sub_protocol"]; }

protected:
  YAODAQ_API nlohmann::json& payload() noexcept { return m_data["payload"]; }
  YAODAQ_API nlohmann::json& meta() noexcept { return m_data["meta"]; }
  nlohmann::json             m_data;
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

class Except : public Message
{
public:
  YAODAQ_API explicit Except() noexcept = delete;
  YAODAQ_API explicit Except( const Exception& exception );
  YAODAQ_API explicit Except( const std::exception& exception );
  YAODAQ_API explicit Except( const std::string_view& exception );
};

}  // namespace yaodaq
