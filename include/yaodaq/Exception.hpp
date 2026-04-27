#pragma once

#include <cstddef>
#include <exception>
#include <string>
#include <yaodaq/Export.hpp>

namespace yaodaq
{

class Exception : public std::exception
{
public:
  YAODAQ_API ~Exception() noexcept override       = default;
  YAODAQ_API explicit Exception( std::nullptr_t ) = delete;
  YAODAQ_API explicit Exception( const char* const message ) : m_message( message ? message : u8"" ) {}
  template<std::size_t N> YAODAQ_API explicit Exception( const char ( &message )[N] ) : m_message( message ) {}
  YAODAQ_API explicit Exception( std::string&& message ) noexcept : m_message( std::move( message ) ) {}
  YAODAQ_API explicit Exception( const std::string& message ) : m_message( message ) {}
  YAODAQ_API [[nodiscard]] const char* what() const noexcept final { return m_message.c_str(); };

private:
  explicit Exception() noexcept = delete;
  const std::string m_message;
};

}  // namespace yaodaq
