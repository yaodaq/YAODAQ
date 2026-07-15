#pragma once
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace yaodaq
{

using ByteBuffer     = std::vector<std::byte>;
using ByteBufferView = std::span<const std::byte>;

struct TransportPacket
{
  TransportPacket( const ByteBufferView payload, const std::string_view channel ) : payload( payload.begin(), payload.end() ), channel( channel ) {}
  void        setCorrelationID( const std::string_view corre ) { correlation_id = corre; }
  ByteBuffer  payload;
  std::string channel;
  std::string correlation_id;
};

}  // namespace yaodaq
