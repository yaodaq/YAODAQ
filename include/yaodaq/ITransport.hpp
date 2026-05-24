#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <vector>

class ITransport
{
public:
  explicit ITransport( nlohmann::json json = nlohmann::json::object() ) : m_json( std::move( json ) ) {}
  virtual ~ITransport() = default;
  virtual bool open()   = 0;
  virtual bool close()  = 0;

  virtual void write( const std::vector<uint8_t>& data ) = 0;

  virtual std::optional<std::vector<uint8_t>> read()             = 0;
  virtual bool                                verifyParameters() = 0;
  void                                        setParameters( nlohmann::json& json ) { m_json = std::move( json ); }
  const nlohmann::json&                       getParameters() const noexcept { return m_json; }
  nlohmann::json&                             getParameters() noexcept { return m_json; }

private:
  nlohmann::json m_json{};
};
