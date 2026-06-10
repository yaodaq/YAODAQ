#pragma once

#include "yaodaq/Export.hpp"
#include "yaodaq/Formatter.hpp"
#include "yaodaq/Identifier.hpp"

#include <string>
#include <string_view>

namespace yaodaq
{

// ============================================================
// Response (raw JSON container)
// ============================================================

class Response
{
public:
  YAODAQ_API explicit Response( const std::string_view response ) : m_raw( response ) {}

  YAODAQ_API std::string_view dump( std::size_t = 0 ) const { return m_raw; }

  YAODAQ_API std::string pretty_format();
  YAODAQ_API std::string tabulate();

protected:
  std::string m_raw;
};

}  // namespace yaodaq
