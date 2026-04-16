#pragma once
#include "yaodaq/Export.hpp"

#include <cstddef>
#include <cstdint>
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
}

namespace yaodaq
{

class Message
{
public:
  enum class Type : std::uint8_t
  {
    Log,
    Open,
  };
  YAODAQ_API explicit Message() noexcept = delete;
  YAODAQ_API explicit Message( const Type type );
  YAODAQ_API std::string dump( const std::size_t i = 0 ) const { return m_data.dump( i ); }
  YAODAQ_API const nlohmann::json& content() const noexcept { return m_data["content"]; }

protected:
  nlohmann::json m_data;
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
};

}  // namespace yaodaq
