#pragma once
#include <string>
#include <string_view>

namespace yaodaq
{

class Connector
{
public:
  explicit Connector( std::string_view type ) : m_type( type ) {};
  virtual ~Connector() noexcept = default;

protected:
  virtual void connect()    = 0;
  virtual void disconnect() = 0;

private:
  std::string m_type;
};

}  // namespace yaodaq
