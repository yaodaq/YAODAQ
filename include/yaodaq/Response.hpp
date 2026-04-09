#pragma once

#include "yaodaq/Identifier.hpp"

#include <cpp-terminal/screen.hpp>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <new>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <yaodaq/Export.hpp>
namespace yaodaq
{

// A response to a JsonRPC
class Response
{
public:
  YAODAQ_API explicit Response( const nlohmann::json& response ) : m_json( response ) {}
  YAODAQ_API std::string_view dump( const std::size_t j ) { return m_json.dump( j ); }
  YAODAQ_API                  operator const nlohmann::json&() const noexcept { return m_json; }
  YAODAQ_API                  operator nlohmann::json() noexcept { return m_json; }
  // Pretty formatter for JSON-RPC array of results/errors
  YAODAQ_API std::string pretty_format();

protected:
  nlohmann::json m_json;
};

// Response where you don't care the server response
class ResponseClients : public Response
{
public:
  YAODAQ_API ResponseClients( const Response& response ) : Response( response )  // uses your conversion operator
  { filter(); }
  YAODAQ_API explicit ResponseClients( const nlohmann::json& response ) : Response( response ) { filter(); }

private:
  void filter()
  {
    for( auto it = m_json.begin(); it != m_json.end(); )
    {
      if( it->is_object() && it->contains( "yaodaq_id" ) && ( *it )["yaodaq_id"].contains( "component" ) && ( *it )["yaodaq_id"]["component"] == "Server" ) { it = m_json.erase( it ); }
      else
      {
        ++it;
      }
    }
  }
};

}  // namespace yaodaq
