#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/Message.hpp"
#include "yaodaq/Parameters.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace yaodaq
{

class ICodec
{
public:
  YAODAQ_API ICodec() noexcept                                                                        = default;
  YAODAQ_API virtual ~ICodec() noexcept                                                               = default;
  YAODAQ_API virtual std::vector<std::byte>           encode( const yaodaq::Message& msg ) const      = 0;
  YAODAQ_API virtual std::unique_ptr<yaodaq::Message> decode( std::span<const std::byte> data ) const = 0;
  YAODAQ_API void                                     setParameters( const Parameters& params ) { m_params = params; }

private:
  Parameters m_params;
};

}  // namespace yaodaq
