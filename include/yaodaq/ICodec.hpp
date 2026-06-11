#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/Logging.hpp"
#include "yaodaq/Message.hpp"
#include "yaodaq/Parameters.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace yaodaq
{

class ICodec : public Loggable
{
public:
  YAODAQ_API ICodec( const std::string_view name, const std::string_view type ) : Loggable( Identifier( Component::Role::Codec, type, name ) ) {}
  YAODAQ_API virtual ~ICodec() noexcept                                                       = default;
  YAODAQ_API virtual std::vector<std::byte>   encode( const Message& msg ) const              = 0;
  YAODAQ_API virtual std::unique_ptr<Message> decode( std::span<const std::byte> data ) const = 0;
  YAODAQ_API void                             setParameters( const Parameters& params ) noexcept { m_params = params; }

private:
  Parameters m_params;
};

}  // namespace yaodaq
