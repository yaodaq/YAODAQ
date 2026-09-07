#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/ICodec.hpp"

namespace yaodaq
{

class DefaultCodec final : public ICodec
{
public:
  YAODAQ_API DefaultCodec() : ICodec( "default", "default" ) {}
  YAODAQ_API virtual ~DefaultCodec() noexcept = default;
  YAODAQ_API virtual std::vector<std::byte> encode( const Message& msg ) const final
  {
    switch( msg.type() )
    {
      case yaodaq::Message::Type::RawData:
      {
        const auto& raw = static_cast<const RawData&>( msg );
        return { raw.payload().begin(), raw.payload().end() };
      }
      default:
      {
        error( "Message is on type {} not RawData !", msg.type_str() );
        return {};
      }
    }
  }
  YAODAQ_API virtual std::unique_ptr<Message> decode( const TransportPacket& data ) const final { return std::make_unique<RawData>( data.payload, data.channel ); }
};

}  // namespace yaodaq
