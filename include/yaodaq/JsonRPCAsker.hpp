#pragma once
#include <cstdint>
#include <cxx/jsonrpc/client.hpp>
#include <string>
#include <yaodaq/Export.hpp>
#include <yaodaq/Response.hpp>

namespace yaodaq
{

class JsonRPCAsker : public jsonrpc::IAsyncClientConnector
{
public:
  YAODAQ_API JsonRPCAsker() : m_client( *this ) {}
  YAODAQ_API virtual ~JsonRPCAsker() noexcept {};

protected:
  YAODAQ_API static std::int64_t generateID();  // nanosecond since epoch
private:
  jsonrpc::JsonRpcClient m_client;

public:
  Response CallMethod( const std::string& name ) { return Response( m_client.CallMethod<nlohmann::json>( std::to_string( generateID() ), name ) ); }
  Response CallMethod( const std::string& name, const jsonrpc::positional_parameter& params ) { return Response( m_client.CallMethod<nlohmann::json>( std::to_string( generateID() ), name, params ) ); }
  Response CallMethodNamed( const std::string& name, const jsonrpc::named_parameter& params = {} ) { return Response( m_client.CallMethodNamed<nlohmann::json>( std::to_string( generateID() ), name, params ) ); }

  YAODAQ_API void CallNotification( const std::string& name, const jsonrpc::positional_parameter& params = {} ) { m_client.CallNotification( name, params ); }
  YAODAQ_API void CallNotificationNamed( const std::string& name, const jsonrpc::named_parameter& params = {} ) { m_client.CallNotificationNamed( name, params ); }
};

}  // namespace yaodaq
