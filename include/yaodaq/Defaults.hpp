#pragma once
#include <cstdint>
#include <limits>
#include <string_view>

namespace yaodaq
{

class defaults
{
public:
  inline static constexpr std::string_view host{ "127.0.0.1" };
  inline static constexpr std::uint16_t    port{ 8888 };
  inline static constexpr std::string_view path{ "/" };
  inline static constexpr std::size_t      max_connections{ ( std::numeric_limits<std::size_t>::max )() };
  inline static constexpr std::size_t      handshake_timeout{ ( std::numeric_limits<std::size_t>::max )() };
  inline static constexpr int              ping_interval_seconds{ -1 };
  inline static constexpr std::size_t      backlog{ ( std::numeric_limits<std::size_t>::max )() };
  inline static constexpr bool             ip6{ 0 };
};

}  // namespace yaodaq
