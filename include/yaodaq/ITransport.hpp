#pragma once
#include "yaodaq/Export.hpp"
#include "yaodaq/Logging.hpp"
#include "yaodaq/Parameters.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace yaodaq
{

class ITransport : public Loggable
{
public:
  YAODAQ_API explicit ITransport( const std::string_view name, const std::string_view type ) : Loggable( Identifier( Component::Role::Transport, type, name ) ) {}
  YAODAQ_API virtual ~ITransport() = default;
  YAODAQ_API virtual bool open()   = 0;
  YAODAQ_API virtual bool close()  = 0;

  YAODAQ_API virtual void write( std::span<const std::byte> data ) = 0;

  YAODAQ_API virtual std::optional<std::vector<std::byte>> read()             = 0;
  YAODAQ_API virtual bool                                  verifyParameters() = 0;
  YAODAQ_API void                                          setParameters( const Parameters& params ) noexcept { m_params = params; }
  YAODAQ_API const Parameters&                             getParameters() const noexcept { return m_params; }
  YAODAQ_API Parameters&                                   getParameters() noexcept { return m_params; }

private:
  Parameters m_params;
};

}  // namespace yaodaq
