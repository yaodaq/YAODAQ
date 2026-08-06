#pragma once

#include <exception>
#include <string>
#include <string_view>

class ReturnValue
{
public:
  explicit ReturnValue() noexcept = default;
  explicit ReturnValue( const std::exception& exception ) noexcept : m_exception( std::make_exception_ptr( exception ) ), m_message( "exception: " + std::string( exception.what() ) ) {}
  explicit ReturnValue( const std::exception_ptr& exception ) noexcept : m_exception( exception )
  {
    try
    {
      if( m_exception ) std::rethrow_exception( m_exception );
    }
    catch( const std::exception& e )
    {
      m_message = "exception: " + std::string( e.what() );
    }
    catch( ... )
    {
      m_message = "exception: thrown object is not derived from std::exception";
    }
  }
  ReturnValue( const std::string_view message ) noexcept : m_message( message ) {}
  ReturnValue( bool success ) noexcept : m_message( success ? "" : "Operation failed" ) {}

  static ReturnValue fromException() noexcept
  {
    ReturnValue r( std::current_exception() );
    return r;
  }

  ReturnValue( const ReturnValue& ) noexcept            = default;
  ReturnValue( ReturnValue&& ) noexcept                 = default;
  ReturnValue& operator=( const ReturnValue& ) noexcept = default;
  ReturnValue& operator=( ReturnValue&& ) noexcept      = default;

  ~ReturnValue() noexcept = default;

  operator bool() const noexcept { return !m_exception && m_message.empty(); }

  operator std::string() const noexcept
  {
    if( !m_message.empty() ) return m_message;
    else
      return "Success";
  }

  bool success() const noexcept { return !m_exception && m_message.empty(); }

  bool hasException() const noexcept { return static_cast<bool>( m_exception ); }

  void rethrowException() const
  {
    if( m_exception ) std::rethrow_exception( m_exception );
  }

private:
  std::exception_ptr m_exception{ nullptr };
  std::string        m_message{};
};
