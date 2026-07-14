#pragma once
#include<vector>
#include<cstddef>
#include<span>
#include<string_view>

namespace yaodaq
{

using ByteBuffer = std::vector<std::byte>;
using ByteBufferView = std::span<const std::byte>;

struct TransportPacket
{
  TransportPacket(const ByteBufferView payload, const std::string_view channel) : payload(payload.begin(), payload.end()), channel(channel) {}
  ByteBuffer payload;
  std::string channel;
}; 

}