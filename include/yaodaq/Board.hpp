#pragma once
#include <yaodaq/Export.hpp>
#include <yaodaq/Identifier.hpp>

namespace yaodaq
{

/**
* @brief A YAODAQ Board is a Module who need to connect
*
**/
class Board
{
public:
  YAODAQ_API explicit Board( const std::string_view name, const std::string_view host = defaults::host, const std::uint16_t port = defaults::port, const std::string_view path = defaults::path, const std::string_view type = "UserType" ) :
    Module( name, url, port, type ), m_identifier( Component::Name::Board, type, name )
  {
  }
  YAODAQ_API explicit Board() noexcept = delete;
  YAODAQ_API virtual ~Board() noexcept = default;

private:
};

}  // namespace yaodaq
