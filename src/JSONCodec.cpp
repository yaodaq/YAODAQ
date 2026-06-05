#include "yaodaq/JSONCodec.hpp"

#include <iostream>

std::vector<std::byte> yaodaq::JSONCodec::encode( const yaodaq::Message& msg ) const
{
  switch( msg.type() )
  {
    case Message::Type::Close:
    case Message::Type::Error:
    case Message::Type::Exception:
    case Message::Type::Log:
    case Message::Type::Open:
    case Message::Type::Ping:
    case Message::Type::Pong:
    case Message::Type::Reject:
    case Message::Type::RPCRequest:
    case Message::Type::RPCResponse:
    {
      break;
    }
    case Message::Type::RawData:
    {
      auto const& raw = dynamic_cast<const RawData&>( msg );
      //std::cout<<"I will send a rawdata :\n"<<std::endl;
      //const std::string_view view(reinterpret_cast<const char*>(raw.raw().data()),raw.raw().size());
      //std::cout<<view<<"\n\n"<<std::endl;

      return { raw.raw().begin(), raw.raw().end() };
    }
    break;
  }

  const std::string      j = msg.dump();
  std::vector<std::byte> out;
  out.reserve( j.size() );

  for( unsigned char c: j ) out.push_back( static_cast<std::byte>( c ) );

  return out;
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
