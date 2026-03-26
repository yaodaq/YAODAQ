#pragma once
#include "yaodaq/Export.hpp"

#include <cstdint>
#include <string>

namespace yaodaq
{

class State
{
public:
  enum class ID : std::uint8_t
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
  YAODAQ_API void setId( const State::ID& id ) { m_id = id; }
  YAODAQ_API State::ID id() const noexcept { return m_id; }
  YAODAQ_API std::string str() const;

private:
  State::ID m_id{ State::ID::Empty };
};

}  // namespace yaodaq
