#pragma once
#include <cstring>
#include <exception>
#include <string>

namespace yaodaq
{

class Exception : public std::exception
{
public:
  ~Exception() noexcept override
  {
    if( m_is_dynamic ) { delete[] m_message; }
  }

  template<std::size_t N> explicit Exception( const char ( &message )[N] ) noexcept { m_message = std::move( message ); }

  explicit Exception( std::string&& message )
  try
  {
    m_message = new char[message.size() + 1];                                            // Allocate memory for the string
    std::memcpy( const_cast<char*>( m_message ), message.c_str(), message.size() + 1 );  // Copy the string's content
  }
  catch( ... )
  {
    throw Exception();
  }

  explicit Exception( const std::string& message )
  try : m_is_dynamic( true )
  {
    m_message = new char[message.size() + 1];                                            // Allocate memory for the string
    std::memcpy( const_cast<char*>( m_message ), message.c_str(), message.size() + 1 );  // Copy the string's content
  }
  catch( ... )
  {
    throw Exception();
  }

  [[nodiscard]] const char* what() const noexcept final
  {
    if( !m_message ) return "Error trying to allocate std::string in yaodaq::Exception";
    else
      return m_message;
  };

private:
  explicit Exception() noexcept = default;
  const char* m_message{ nullptr };
  bool        m_is_dynamic{ false };
};

}  // namespace yaodaq
