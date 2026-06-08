#include <doctest/doctest.h>
#include <exception>
#include <iostream>
#include <yaodaq/Message.hpp>

TEST_CASE( "Message with bad jsons" )
{
  //const yaodaq::Message empty( nlohmann::json::object() );
  //CHECK_EQ( empty.payload().is_object(), true );
  //CHECK_EQ( empty.meta().is_object(), true );
  //CHECK_EQ(empty.meta()["type"],"Unknown");
  //CHECK_EQ(empty.type(),"Unknown");
}

TEST_CASE( "Except" )
{
  const yaodaq::Except yaodaq_exception( yaodaq::Exception( "My exception" ) );
  //CHECK_EQ( yaodaq_exception.payload().is_object(), true );
  //CHECK_EQ( yaodaq_exception.meta().is_object(), true );
  //CHECK_EQ(yaodaq_exception.meta()["type"],"Exception");
  CHECK_EQ( yaodaq_exception.what(), "My exception" );

  const yaodaq::Except std_exception( std::runtime_error( "from std::exception" ) );
  //CHECK_EQ( std_exception.payload().is_object(), true );
  //CHECK_EQ( std_exception.meta().is_object(), true );
  //CHECK_EQ(std_exception.meta()["type"],"Exception");
  CHECK_EQ( std_exception.what(), "from std::exception" );

  const yaodaq::Except string_exception( "string exception" );
  //CHECK_EQ( string_exception.payload().is_object(), true );
  //CHECK_EQ( string_exception.meta().is_object(), true );
  //CHECK_EQ(string_exception.meta()["type"],"Exception");
  CHECK_EQ( string_exception.what(), "string exception" );
}
