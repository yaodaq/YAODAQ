#pragma once
#include "yaodaq/Export.hpp"

#include <cstdint>
#include <string>

namespace yaodaq
{

class State
{
public:
  enum class Type : std::uint8_t
  {
    Empty = 0,
    Linked,
    Initialized,
    Connected,
    Configured,
    Started,
    Paused,
    Stopped,
    Cleared,
    Disconnected,
    Released,
  };
  using underlying = std::underlying_type_t<State::Type>;
  YAODAQ_API void                  setId( const State::Type id ) noexcept { m_type = id; }
  YAODAQ_API constexpr State::Type type() const noexcept { return m_type; }
  YAODAQ_API std::string str() const;
  friend bool            operator==( const State& lhs, State::Type rhs ) noexcept { return lhs.m_type == rhs; }
  friend bool            operator!=( const State& lhs, State::Type rhs ) noexcept { return !( lhs == rhs ); }

private:
  State::Type m_type{ State::Type::Empty };
};

}  // namespace yaodaq
