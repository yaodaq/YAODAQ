#include "yaodaq/Component.hpp"

#include "magic_enum/magic_enum.hpp"

std::string yaodaq::Component::str() const { return std::string( magic_enum::enum_name( m_component ) ); }
