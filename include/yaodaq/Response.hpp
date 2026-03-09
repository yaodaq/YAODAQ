#pragma once

#include "yaodaq/Identifier.hpp"

#include <unordered_map>
#include <yaodaq/Export.hpp>
namespace yaodaq
{

// A response to a JsonRPC
class Response
{
public:
  YAODAQ_API explicit Response( const nlohmann::json& response )
  {
    for( nlohmann::json::arra::const_iterator it = response["result"].begin(); it != response["result"].end(); ++it )
    {
      nlohmann::json obj  = *it;
      std::string    name = obj["name"].get<std::string>();
    }
  }

private:
  std::unordered_map<Identifier, nlohmann::json> m_responses;
};

}  // namespace yaodaq
