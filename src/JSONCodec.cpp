#include "yaodaq/JSONCodec.hpp"

#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <simdjson.h>

std::vector<std::byte> yaodaq::JSONCodec::encode( const yaodaq::Message& msg ) const
{
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
  if( data.empty() ) return nullptr;
  thread_local simdjson::dom::parser parser;
  auto                               doc = parser.parse( reinterpret_cast<const char*>( data.data() ), data.size() );
  if( doc.error() ) throw std::runtime_error( simdjson::error_message( doc.error() ) );
  simdjson::dom::element json = doc.value();

  //
  // RPC fast-path
  //
  if( !json["method"].error() || !json["notification"].error() )
  {
    std::unique_ptr<RPCRequest> msg     = std::make_unique<RPCRequest>( simdjson::minify( json ) );
    auto                        id_elem = json["id"];
    if( id_elem.type() == simdjson::dom::element_type::STRING ) msg->id( std::string( json["id"] ) );
    else
      msg->id( std::int64_t( json["id"] ) );
    return msg;
  }
  if( !json["result"].error() || !json["error"].error() )
  {
    std::unique_ptr<RPCResponse> msg     = std::make_unique<RPCResponse>( simdjson::minify( json ) );
    auto                         id_elem = json["id"];
    if( id_elem.type() == simdjson::dom::element_type::STRING ) msg->id( std::string( json["id"] ) );
    else
      msg->id( std::int64_t( json["id"] ) );
    return msg;
  }

  //
  // Raw payload
  //
  auto meta = json["meta"];
  if( meta.error() ) return std::make_unique<RawData>( data, "unknown" );

  std::string_view type_str;
  if( meta["type"].get( type_str ) ) return std::make_unique<RawData>( data, "unknown" );

  auto type = magic_enum::enum_cast<Message::Type>( type_str, magic_enum::case_insensitive ).value_or( Message::Type::Unknown );

  //
  // common metadata
  //
  std::string_view uuid;
  std::int64_t     time{ 0 };

  meta["uuid"].get( uuid );
  meta["time"].get( time );

  std::uint64_t major{};
  std::uint64_t minor{};
  std::uint64_t patch{};
  std::uint64_t tweak{};

  auto version = meta["yaodaq"]["version"];

  version["major"].get( major );
  version["minor"].get( minor );
  version["patch"].get( patch );
  version["tweak"].get( tweak );

  Version ver{ major, minor, patch, tweak };

  auto                     payload = json["payload"];
  std::unique_ptr<Message> msg;

  switch( type )
  {
    case Message::Type::Close:
    {
      std::uint64_t    code{};
      bool             remote{};
      std::string_view reason;

      payload["code"].get( code );
      payload["reason"].get( reason );
      payload["remote"].get( remote );

      msg = std::make_unique<Close>( static_cast<std::uint16_t>( code ), reason, remote );
      break;
    }
    case Message::Type::Reject:
    {
      std::uint64_t    code{};
      bool             remote{};
      std::string_view reason;

      payload["code"].get( code );
      payload["reason"].get( reason );
      payload["remote"].get( remote );

      msg = std::make_unique<Reject>( static_cast<std::uint16_t>( code ), reason, remote );
      break;
    }
    case Message::Type::Error:
    {
      std::string_view reason;
      std::uint64_t    retries{};
      double           wait_time{};
      std::int64_t     http_status{};
      bool             decompression{};

      payload["reason"].get( reason );
      payload["retries"].get( retries );
      payload["wait_time"].get( wait_time );
      payload["http_status"].get( http_status );
      payload["decompression_error"].get( decompression );

      msg = std::make_unique<Error>( reason, static_cast<std::uint32_t>( retries ), wait_time, static_cast<int>( http_status ), decompression );
      break;
    }
    case Message::Type::Exception:
    {
      std::string_view what;
      std::string_view exception_type;
      payload["what"].get( what );
      payload["exception_type"].get( exception_type );

      msg = std::make_unique<Except>( what, exception_type );
      break;
    }
    case Message::Type::Ping:
    {
      std::string_view message;
      std::uint64_t    size{};
      bool             binary{};

      payload["message"].get( message );
      payload["size"].get( size );
      payload["binary"].get( binary );

      msg = std::make_unique<Ping>( message, static_cast<std::size_t>( size ), binary );
      break;
    }
    case Message::Type::Pong:
    {
      std::string_view message;
      std::uint64_t    size{};
      bool             binary{};

      payload["message"].get( message );
      payload["size"].get( size );
      payload["binary"].get( binary );

      msg = std::make_unique<Pong>( message, static_cast<std::size_t>( size ), binary );
      break;
    }
    case Message::Type::Open:
    {
      std::string_view uri;
      std::string_view protocol;

      payload["uri"].get( uri );
      payload["protocol"].get( protocol );

      std::map<std::string, std::string> headers;

      for( auto field: payload["headers"].get_object() )
      {
        std::string_view value;
        if( !field.value.get( value ) ) headers.emplace( std::string( field.key ), std::string( value ) );
      }

      msg = std::make_unique<Open>( uri, headers, protocol );
      break;
    }
    case Message::Type::Log:
    {
      std::string_view logger_name;
      std::string_view message;
      std::string_view filename;
      std::string_view funcname;

      std::int64_t  logger_time{};
      std::int64_t  level{};
      std::uint64_t line{};
      std::uint64_t column{};

      payload["logger_name"].get( logger_name );
      payload["message"].get( message );
      payload["time"].get( logger_time );
      payload["level"].get( level );

      payload["source_loc"]["filename"].get( filename );
      payload["source_loc"]["funcname"].get( funcname );

      payload["source_loc"]["line"].get( line );

      payload["source_loc"]["column"].get( column );

      msg = std::make_unique<Log>( logger_name, static_cast<int>( level ), message, logger_time, filename, funcname, static_cast<std::uint32_t>( line ), static_cast<std::uint32_t>( column ) );
      break;
    }
    default: return std::make_unique<RawData>( data, "unknown" );
  }
  msg->setMeta( uuid, time, ver );
  return msg;
}
