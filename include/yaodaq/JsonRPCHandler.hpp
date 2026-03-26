#pragma once
#include <cxx/jsonrpc/client.hpp>
#include <cxx/jsonrpc/server.hpp>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <yaodaq/Export.hpp>
#include <yaodaq/Identifier.hpp>
#include <yaodaq/Response.hpp>

namespace yaodaq
{

class JsonRPCHandler : public jsonrpc::IAsyncClientConnector
{
public:
  YAODAQ_API JsonRPCHandler() : m_client( *this ) { Add( "listProcedures", GetHandle( &yaodaq::JsonRPCHandler::getProcedures, *this ) ); }
  YAODAQ_API virtual ~JsonRPCHandler() noexcept {};

protected:
  YAODAQ_API static std::int64_t generateID();  // nanosecond since epoch
private:
  jsonrpc::JsonRpcClient m_client;
  jsonrpc::JsonRpcServer m_server;

public:
  YAODAQ_API bool Add( const std::string& name, jsonrpc::Method callback, const std::initializer_list<std::string>& mapping = {} ) { return m_server.Add( name, std::move( callback ), mapping ); }
  YAODAQ_API bool Add( const std::string& name, jsonrpc::Notification callback, const std::initializer_list<std::string>& mapping = {} ) { return m_server.Add( name, std::move( callback ), mapping ); }
  Response        CallMethod( const std::string& name ) { return Response( m_client.CallMethod<nlohmann::json>( std::to_string( generateID() ), name ) ); }
  Response        CallMethod( const std::string& name, const jsonrpc::positional_parameter& params ) { return Response( m_client.CallMethod<nlohmann::json>( std::to_string( generateID() ), name, params ) ); }
  Response        CallMethodNamed( const std::string& name, const jsonrpc::named_parameter& params = {} ) { return Response( m_client.CallMethodNamed<nlohmann::json>( std::to_string( generateID() ), name, params ) ); }

  YAODAQ_API void CallNotification( const std::string& name, const jsonrpc::positional_parameter& params = {} ) { m_client.CallNotification( name, params ); }
  YAODAQ_API void CallNotificationNamed( const std::string& name, const jsonrpc::named_parameter& params = {} ) { m_client.CallNotificationNamed( name, params ); }

  YAODAQ_API nlohmann::json getProcedures() { return m_server.getProcedures(); }

protected:
  YAODAQ_API std::string HandleRequest( const nlohmann::json& requestString ) { return m_server.HandleRequest( requestString.dump() ); }
};

}  // namespace yaodaq
