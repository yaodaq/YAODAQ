#include "yaodaq/Component.hpp"
#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 256
#include "magic_enum/magic_enum.hpp"

#include <string>

yaodaq::Component::operator std::string() const { return std::string( magic_enum::enum_name( m_role ) ); }
