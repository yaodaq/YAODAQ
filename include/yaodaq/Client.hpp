#pragma once
#include <ixwebsocket/IXWebSocket.h>
#include <string>
#include <yaodaq/Identifier.hpp>
#include <yaodaq/JsonRPCHandler.hpp>
#include <yaodaq/Export.hpp>

namespace yaodaq
{

class Client : public ix::WebSocket, public JsonRPCHandler
{
public:
  YAODAQ_API Client( const std::string& name, const std::string& url = "127.0.0.1", const int port = 8080, const std::string& type = "YAODAQ" );
  YAODAQ_API void setTLS( const std::string& certFile, const std::string& keyFile, const std::string& caFile = "SYSTEM" )
  {
    m_tlsOptions.certFile = certFile;
    m_tlsOptions.keyFile  = keyFile;
    m_tlsOptions.caFile   = caFile;
    m_tlsOptions.tls      = true;
    setTLSOptions( m_tlsOptions );
  }
  YAODAQ_API virtual ~Client() noexcept;

  YAODAQ_API const Identifier& identifier() const noexcept { return m_identifier; }

protected:
  Identifier m_identifier{ Component::Name::Client };

private:
  ix::SocketTLSOptions m_tlsOptions;
  /* Check if the client has been rejected*/
  void                 checkReject( const ix::WebSocketCloseInfo& close_info );

  void onMessage( const std::string& str, const std::size_t size, const bool binary );
  void onFragment( const std::string& str, const std::size_t size, const bool binary );
  void onOpen( const std::string& uri, const ix::WebSocketHttpHeaders& headers, const std::string& protocol );
  void onClose( const std::uint16_t code, const std::string& reason, bool remote );
  void onError( const std::uint32_t retries, const double wait_time, const int http_status, const std::string& reason, const bool decompressionError );
  void onPing( const std::string& str, const std::size_t size, const bool binary );
  void onPong( const std::string& str, const std::size_t size, const bool binary );

  void onJsonRPC( const nlohmann::json& json );
  void onText( const std::string& text );
  void onJson( const nlohmann::json& json );

  virtual std::string Send( const std::string& request ) override final;
};

}  // namespace yaodaq
