#include "yaodaq/Component.hpp"

#include "magic_enum/magic_enum.hpp"

#include <string>

yaodaq::Component::operator std::string() const { return std::string( magic_enum::enum_name( m_role ) ); }
