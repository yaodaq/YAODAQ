#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/ICodec.hpp"

namespace yaodaq
{

//TODO SPLIT that shit
class YAODAQ_API JSONCodec final : public ICodec
{
public:
  YAODAQ_API JSONCodec() : ICodec( "default", "JSON" ) {}
  YAODAQ_API ~JSONCodec() override = default;
  YAODAQ_API std::vector<std::byte> encode( const yaodaq::Message& msg ) const override;
  YAODAQ_API std::unique_ptr<yaodaq::Message> decode( const TransportPacket& data ) const override;
};

class YAODAQ_API YAODAQJSONCodec final : public ICodec
{
public:
  YAODAQ_API YAODAQJSONCodec() : ICodec( "YAODAQ", "JSON" ) {}
  YAODAQ_API ~YAODAQJSONCodec() override = default;
  YAODAQ_API std::vector<std::byte> encode( const yaodaq::Message& msg ) const override;
  YAODAQ_API std::unique_ptr<yaodaq::Message> decode( const TransportPacket& data ) const override;
};

}  // namespace yaodaq
