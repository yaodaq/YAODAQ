#pragma once
#include <cxx/jsonrpc/server.hpp>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <yaodaq/Export.hpp>
#include <yaodaq/Identifier.hpp>
#include <yaodaq/Response.hpp>

namespace yaodaq
{

class JsonRPCResponder
{
public:
  YAODAQ_API JsonRPCResponder() { Add( "listProcedures", jsonrpc::GetHandle( &yaodaq::JsonRPCResponder::getProcedures, *this ) ); }
  YAODAQ_API virtual ~JsonRPCResponder() noexcept {};

private:
  jsonrpc::JsonRpcServer m_server;

public:
  YAODAQ_API bool Add( const std::string& name, jsonrpc::Method callback, const std::initializer_list<std::string>& mapping = {} ) { return m_server.Add( name, std::move( callback ), mapping ); }
  YAODAQ_API bool Add( const std::string& name, jsonrpc::Notification callback, const std::initializer_list<std::string>& mapping = {} ) { return m_server.Add( name, std::move( callback ), mapping ); }

  YAODAQ_API nlohmann::json getProcedures() { return m_server.getProcedures(); }

protected:
  YAODAQ_API std::string HandleRequest( const nlohmann::json& requestString ) { return m_server.HandleRequest( requestString.dump() ); }
};

}  // namespace yaodaq
