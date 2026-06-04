#include "yaodaq/JSONCodec.hpp"

std::vector<std::byte> yaodaq::JSONCodec::encode( const yaodaq::Message& msg ) const
{
  const std::string      j = msg.dump();
  std::vector<std::byte> out;
  out.reserve( j.size() );

  for( unsigned char c: j ) out.push_back( static_cast<std::byte>( c ) );

  return out;
}

std::unique_ptr<yaodaq::Message> yaodaq::JSONCodec::decode( std::span<const std::byte> data ) const
{
  if( data.empty() ) throw std::runtime_error( "Empty payload" );
  const auto* begin = reinterpret_cast<const char*>( data.data() );

  const auto* end = begin + data.size();

  nlohmann::json json = nlohmann::json::parse( begin, end, nullptr,
                                               true  // throw on error
  );

  return std::make_unique<Message>( json );
}
