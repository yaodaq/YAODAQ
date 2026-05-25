#pragma once

#include "yaodaq/Message.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace yaodaq
{

class ICodec
{
public:
  virtual ~ICodec() = default;

  virtual std::vector<std::uint8_t> encode( const yaodaq::Message& msg ) const = 0;

  virtual yaodaq::Message decode( std::string_view data ) const = 0;

  yaodaq::Message decode( const std::vector<std::uint8_t>& data ) const { return decode( std::string_view( reinterpret_cast<const char*>( data.data() ), data.size() ) ); }
};

class Json final : public ICodec
{
public:
  std::vector<std::uint8_t> encode( const yaodaq::Message& msg ) const override
  {
    const auto& j = msg.dump();  // assume std::string or json convertible string

    return std::vector<std::uint8_t>( j.begin(), j.end() );
  }

  yaodaq::Message decode( std::string_view data ) const override
  {
    // zero-copy parse from view (no std::string allocation)
    auto json = nlohmann::json::parse( data.data(), data.data() + data.size(), nullptr, false );

    return Message( std::move( json ) );
  }
};

enum class codec_type
{
  json = 0,
};

inline std::unique_ptr<ICodec> make_codec( codec_type type = codec_type::json )
{
  switch( type )
  {
    case codec_type::json: return std::make_unique<Json>();
  }

  throw std::runtime_error( "Unhandled codec_type" );
}

}  // namespace yaodaq
