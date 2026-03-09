#pragma once
#include <future>
#include <iostream>
#include <jsonrpccxx/batchclient.hpp>
#include <jsonrpccxx/server.hpp>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <yaodaq/Identifier.hpp>
#include <yaodaq/Export.hpp>
namespace yaodaq
{

struct ServerRequest
{
  std::mutex                                      mtx;  // protect shared state
  std::condition_variable                         cv;
  std::unordered_map<std::string, nlohmann::json> responses;
  size_t                                          expected_responses{ 0 };
  size_t                                          received_responses{ 0 };
  void                                            print()
  {
    std::cout << "expected: " << expected_responses << std::endl;
    std::cout << "received_responses: " << received_responses << std::endl;
    std::cout << "responses: " << std::endl;
    for( std::unordered_map<std::string, nlohmann::json>::const_iterator it = responses.begin(); it != responses.end(); ++it ) { std::cout << it->first << "  : " << it->second << std::endl; }
  }
};

class JsonRPCHandler : public jsonrpccxx::IClientConnector
{
public:
  YAODAQ_API explicit JsonRPCHandler() noexcept : m_client( *this ) {}
  YAODAQ_API virtual ~JsonRPCHandler() noexcept = default;
  YAODAQ_API bool                   Add( const std::string& name, jsonrpccxx::MethodHandle callback, const jsonrpccxx::NamedParamMapping& mapping = std::vector<std::string>{} ) { return m_server.Add( name, callback, mapping ); }
  YAODAQ_API bool                   Add( const std::string& name, jsonrpccxx::NotificationHandle callback, const jsonrpccxx::NamedParamMapping& mapping = std::vector<std::string>{} ) { return m_server.Add( name, callback, mapping ); }
  template<typename T> T CallMethod( const std::string& name ) { return m_client.CallMethod<T>( std::to_string( generateID() ), name ); }
  template<typename T> T CallMethod( const std::string& name, const jsonrpccxx::positional_parameter& params ) { return m_client.CallMethod<T>( std::to_string( generateID() ), name, params ); }
  template<typename T> T CallMethodNamed( const std::string& name, const jsonrpccxx::named_parameter& params = {} ) { return m_client.CallMethodNamed<T>( std::to_string( generateID() ), name, params ); }

  YAODAQ_API void CallNotification( const std::string& name, const jsonrpccxx::positional_parameter& params = {} ) { m_client.CallNotification( name, params ); }
  YAODAQ_API void CallNotificationNamed( const std::string& name, const jsonrpccxx::named_parameter& params = {} ) { m_client.CallNotificationNamed( name, params ); }

  YAODAQ_API bool isRequest( const nlohmann::json& json ) const noexcept;

  YAODAQ_API bool isResponse( const nlohmann::json& json ) const noexcept;
  YAODAQ_API bool isResult( const nlohmann::json& json ) const noexcept;
  YAODAQ_API bool isError( const nlohmann::json& json ) const noexcept;

  std::mutex                                                            m_mutex;
  std::unordered_map<jsonrpccxx::id_type, std::promise<nlohmann::json>> m_responses;

  std::mutex                                                              m_map_mutex;
  std::unordered_map<jsonrpccxx::id_type, std::shared_ptr<ServerRequest>> m_server_construct_response;

protected:
  std::string HandleRequest( const nlohmann::json& requestString ) { return m_server.HandleRequest( requestString.dump() ); }

private:
  static std::int64_t        generateID();  // nanosecond since
  jsonrpccxx::BatchClient    m_client;
  jsonrpccxx::JsonRpc2Server m_server;
};

}  // namespace yaodaq
