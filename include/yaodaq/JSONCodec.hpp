#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/ICodec.hpp"
namespace yaodaq
{

class YAODAQ_API JSONCodec final : public ICodec
{
public:
  YAODAQ_API ~JSONCodec() override = default;
  YAODAQ_API std::vector<std::byte> encode( const yaodaq::Message& msg ) const override;
  YAODAQ_API std::unique_ptr<yaodaq::Message> decode( std::span<const std::byte> data ) const override;
};

}  // namespace yaodaq
