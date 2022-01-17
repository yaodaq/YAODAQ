/**
\copyright Copyright 2022 flagarde
*/

#include "yaodaq/Identifier.hpp"

#include "doctest/doctest.h"

TEST_CASE("Identifier")
{
  yaodaq::Identifier id(yaodaq::Class::WebSocketServer, "MyType", "MyName");
  CHECK_EQ(id.getName(), "MyName");
  CHECK_EQ(id.getType(), "MyType");
  CHECK_EQ(id.getClass(), "WebSocketServer");
  CHECK_EQ(id.getClassId(), yaodaq::Class::WebSocketServer);
}