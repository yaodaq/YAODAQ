#include "yaodaq/State.hpp"

#include "magic_enum/magic_enum.hpp"

std::string yaodaq::State::str() const { return std::string( magic_enum::enum_name( m_id ) ); }
