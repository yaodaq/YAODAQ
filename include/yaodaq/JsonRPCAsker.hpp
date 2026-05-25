#pragma once
#include <cstdint>
#include <cxx/jsonrpc/client.hpp>
#include <ixwebsocket/IXUuid.h>
#include <string>
#include <yaodaq/Export.hpp>
#include <yaodaq/Response.hpp>

namespace yaodaq
{

class JsonRPCAsker : public jsonrpc::IAsyncClientConnector
{
public:
  YAODAQ_API JsonRPCAsker() : m_client( *this ) { setTimeout( std::chrono::milliseconds( 5000 ) ); }
  YAODAQ_API virtual ~JsonRPCAsker() noexcept {};

private:
  jsonrpc::JsonRpcClient m_client;

public:
  Response CallMethod( const std::string& name ) { return Response( m_client.CallMethod<nlohmann::json>( ix::uuid4(), name ) ); }
  Response CallMethod( const std::string& name, const jsonrpc::positional_parameter& params ) { return Response( m_client.CallMethod<nlohmann::json>( ix::uuid4(), name, params ) ); }
  Response CallMethodNamed( const std::string& name, const jsonrpc::named_parameter& params = {} ) { return Response( m_client.CallMethodNamed<nlohmann::json>( ix::uuid4(), name, params ) ); }

  YAODAQ_API void CallNotification( const std::string& name, const jsonrpc::positional_parameter& params = {} ) { m_client.CallNotification( name, params ); }
  YAODAQ_API void CallNotificationNamed( const std::string& name, const jsonrpc::named_parameter& params = {} ) { m_client.CallNotificationNamed( name, params ); }
};

}  // namespace yaodaq
