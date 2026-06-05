#include "yaodaq/JSONCodec.hpp"

#include <iostream>

std::vector<std::byte> yaodaq::JSONCodec::encode( const yaodaq::Message& msg ) const
{
  std::cout << "use encode type:" << msg.type_str() << std::endl;
  nlohmann::json json;
  json["meta"]                               = nlohmann::json::object();
  json["meta"]["time"]                       = msg.time();
  json["meta"]["uuid"]                       = msg.uuid();
  json["meta"]["type"]                       = msg.type_str();
  json["meta"]["yaodaq"]["version"]["major"] = msg.version().major();
  json["meta"]["yaodaq"]["version"]["minor"] = msg.version().minor();
  json["meta"]["yaodaq"]["version"]["patch"] = msg.version().patch();
  json["meta"]["yaodaq"]["version"]["tweak"] = msg.version().tweak();
  json["payload"]                            = nlohmann::json::object();
  switch( msg.type() )
  {
    case Message::Type::Close:
    {
      const auto& close         = static_cast<const Close&>( msg );
      json["payload"]["code"]   = close.code();
      json["payload"]["reason"] = close.reason();
      json["payload"]["remote"] = close.remote();
      break;
    }
    case Message::Type::Reject:
    {
      const auto& reject        = static_cast<const Reject&>( msg );
      json["payload"]["code"]   = reject.code();
      json["payload"]["reason"] = reject.reason();
      json["payload"]["remote"] = reject.remote();
      break;
    }
    case Message::Type::Error:
    {
      const auto& error                      = static_cast<const Error&>( msg );
      json["payload"]["retries"]             = error.retries();
      json["payload"]["wait_time"]           = error.wait_time();
      json["payload"]["http_status"]         = error.http_status();
      json["payload"]["reason"]              = error.reason();
      json["payload"]["decompression_error"] = error.decompression_error();
      break;
    }
    case Message::Type::Exception:
    {
      const auto& exception             = static_cast<const Except&>( msg );
      json["payload"]["what"]           = exception.what();
      json["payload"]["exception_type"] = exception.exception_type();
      break;
    }
    case Message::Type::Log:
    {
      const auto& log                           = static_cast<const Log&>( msg );
      json["payload"]["logger_name"]            = log.name();
      json["payload"]["level"]                  = log.level();
      json["payload"]["message"]                = log.message();
      json["payload"]["time"]                   = log.logger_time();
      json["payload"]["source_loc"]["filename"] = log.file_name();
      json["payload"]["source_loc"]["funcname"] = log.function_name();
      json["payload"]["source_loc"]["line"]     = log.line();
      break;
    }
    case Message::Type::Open:
    {
      const auto& open            = static_cast<const Open&>( msg );
      json["payload"]["uri"]      = open.uri();
      json["payload"]["headers"]  = open.headers();
      json["payload"]["protocol"] = open.protocol();
      break;
    }
    case Message::Type::Ping:
    {
      const auto& ping           = static_cast<const Ping&>( msg );
      json["payload"]["message"] = ping.message();
      json["payload"]["binary"]  = ping.binary();
      json["payload"]["size"]    = ping.size();
      break;
    }
    case Message::Type::Pong:
    {
      const auto& pong           = static_cast<const Pong&>( msg );
      json["payload"]["message"] = pong.message();
      json["payload"]["binary"]  = pong.binary();
      json["payload"]["size"]    = pong.size();
      break;
    }
    case Message::Type::RPCRequest:
    {
      const auto& request = static_cast<const RPCRequest&>( msg );
      json["payload"]     = request.raw();
      break;
    }
    case Message::Type::RPCResponse:
    {
      const auto& response = static_cast<const RPCResponse&>( msg );
      json["payload"]      = response.raw();
      break;
    }
    case Message::Type::RawData:
    {
      auto const& raw = dynamic_cast<const RawData&>( msg );  //TODO ADD A WAY TO SAVE THE topic
      //std::cout<<"I will send a rawdata :\n"<<std::endl;
      //const std::string_view view(reinterpret_cast<const char*>(raw.raw().data()),raw.raw().size());
      //std::cout<<view<<"\n\n"<<std::endl;

      return { raw.raw().begin(), raw.raw().end() };
      break;
    }
  }

  const std::string j = json.dump();

  return { reinterpret_cast<const std::byte*>( j.data() ), reinterpret_cast<const std::byte*>( j.data() + j.size() ) };
}

std::unique_ptr<yaodaq::Message> yaodaq::JSONCodec::decode( std::span<const std::byte> data ) const
{
  if( data.empty() ) throw std::runtime_error( "Empty payload" );
  // 1. Convert once to string_view (no copy)
  const std::string view( reinterpret_cast<const char*>( data.data() ), data.size() );

  //std::cout<<"ME:\n"<<view<<"\nEND\n\n"<<std::endl;

  // 2. Parse JSON from view
  nlohmann::json json = nlohmann::json::parse( view, nullptr, true );
  // parse failure → discarded JSON (nullptr or discarded value)
  if( json.is_discarded() )
  {
    std::cout << "OUPS     " << view << std::endl;
    //return std::make_unique<yaodaq::RawData>(yaodaq::RawDataBuilder::from_text(view, "invalid-json"));
    return nullptr;
  }

  // 3. Fast-path: missing metadata → treat as raw
  const auto metaIt = json.find( "meta" );
  if( metaIt == json.end() || !metaIt->contains( "type" ) )
  {
    //std::cout<<"IM A RAWDATA\n\n"<< <<std::endl;
    return std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( json.dump(), "unknown" ) );
  }

  // 4. Normal path
  return std::make_unique<yaodaq::Message>( json );
}
