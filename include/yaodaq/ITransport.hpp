#pragma once
#include "yaodaq/Export.hpp"

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <span>
#include <vector>

class ITransport
{
public:
  YAODAQ_API explicit ITransport( nlohmann::json json = nlohmann::json::object() ) : m_json( std::move( json ) ) {}
  YAODAQ_API virtual ~ITransport() = default;
  YAODAQ_API virtual bool open()   = 0;
  YAODAQ_API virtual bool close()  = 0;

  YAODAQ_API virtual void write( std::span<const std::byte> data ) = 0;

  YAODAQ_API virtual std::optional<std::vector<std::byte>> read()             = 0;
  YAODAQ_API virtual bool                                  verifyParameters() = 0;
  YAODAQ_API void                                          setParameters( nlohmann::json& json ) { m_json = std::move( json ); }
  YAODAQ_API const nlohmann::json& getParameters() const noexcept { return m_json; }
  YAODAQ_API nlohmann::json& getParameters() noexcept { return m_json; }

private:
  nlohmann::json m_json{};
};
