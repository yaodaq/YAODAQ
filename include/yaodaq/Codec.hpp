#pragma once
#include "ICodec.hpp"
#include "JSONCodec.hpp"
#include "yaodaq/Export.hpp"

namespace yaodaq
{

enum class codec_type
{
  json = 0,
};

inline std::unique_ptr<ICodec> make_codec( codec_type type = codec_type::json )
{
  switch( type )
  {
    case codec_type::json: return std::make_unique<JSONCodec>();
  }

  throw std::runtime_error( "Unhandled codec_type" );
}

}  // namespace yaodaq
