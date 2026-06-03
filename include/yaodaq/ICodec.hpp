#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/Message.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace yaodaq
{

class ICodec
{
public:
  ICodec() noexcept                                                              = default;
  virtual ~ICodec() noexcept                                                     = default;
  virtual std::vector<std::byte> encode( const yaodaq::Message& msg ) const      = 0;
  virtual yaodaq::Message        decode( std::span<const std::byte> data ) const = 0;
};

class Json final : public ICodec
{
public:
  std::vector<std::byte> encode( const yaodaq::Message& msg ) const override
  {
    const std::string j = msg.dump();

    std::vector<std::byte> out;
    out.reserve( j.size() );

    for( unsigned char c: j ) out.push_back( static_cast<std::byte>( c ) );

    return out;
  }

  yaodaq::Message decode( std::span<const std::byte> data ) const override
  {
    if( data.empty() ) throw std::runtime_error( "Empty payload" );

    const auto* begin = reinterpret_cast<const char*>( data.data() );

    const auto* end = begin + data.size();

    nlohmann::json json = nlohmann::json::parse( begin, end, nullptr,
                                                 true  // throw on error
    );

    return yaodaq::Message( json );
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
