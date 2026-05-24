#pragma once

#include "yaodaq/Message.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace yaodaq
{

class ICodec
{
public:
  virtual ~ICodec()                                                      = default;
  virtual std::vector<std::uint8_t> encode( const yaodaq::Message& msg ) = 0;
  yaodaq::Message                   decode( const std::string& data )
  {
    auto* ptr = reinterpret_cast<const std::uint8_t*>( data.data() );

    return decode( std::vector<std::uint8_t>( ptr, ptr + data.size() ) );
  }
  virtual yaodaq::Message decode( const std::vector<uint8_t>& data ) = 0;
};

class Json final : public ICodec
{
public:
  virtual std::vector<std::uint8_t> encode( const yaodaq::Message& msg )
  {
    const std::string ret{ msg.dump() };
    return { ret.begin(), ret.end() };
  }
  virtual yaodaq::Message decode( const std::vector<uint8_t>& data )
  {
    const std::string ret{ data.begin(), data.end() };
    return Message( nlohmann::json::parse( ret, nullptr, false ) );
  }
};

enum class codec_type
{
  json = 0,
};

static std::unique_ptr<yaodaq::ICodec> make_codec( const codec_type type = codec_type::json )
{
  switch( type )
  {
    case codec_type::json:
    {
      return std::make_unique<Json>();
    }
  }
  throw std::runtime_error( "Unhandled codec_type" );
}

}  // namespace yaodaq
