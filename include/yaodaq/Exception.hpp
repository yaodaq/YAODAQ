#pragma once

#include <exception>
#include <string>
#include <yaodaq/Export.hpp>

namespace yaodaq
{

class ExceptionAllocationProblem : public std::exception
{
public:
  YAODAQ_API ExceptionAllocationProblem() noexcept           = default;
  YAODAQ_API ~ExceptionAllocationProblem() noexcept override = default;
  YAODAQ_API [[nodiscard]] const char* what() const noexcept final { return "Problem while allocating an yaodaq::Exception"; }
};

class Exception : public std::exception
{
public:
  YAODAQ_API ~Exception() noexcept override = default;

  explicit Exception( const char* const message )
  try : m_message( message ) {}
  catch( ... )
  {
    throw m_problem_allocation;
  }

  template<std::size_t N> explicit Exception( const char ( &message )[N] )
  try : m_message( message ) {}
  catch( ... )
  {
    throw m_problem_allocation;
  }

  YAODAQ_API explicit Exception( std::string&& message ) noexcept : m_message( std::move( message ) ) {}

  YAODAQ_API explicit Exception( const std::string& message )
  try
  {
  }
  catch( ... )
  {
    throw m_problem_allocation;
  }

  YAODAQ_API [[nodiscard]] const char* what() const noexcept final { return m_message.data(); };

private:
  const static ExceptionAllocationProblem m_problem_allocation;
  explicit Exception() noexcept = default;
  const std::string m_message;
};

}  // namespace yaodaq
