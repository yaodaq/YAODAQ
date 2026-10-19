#pragma once
#include "serial_cpp/serial.h"
#include "yaodaq/Export.hpp"
#include "yaodaq/ITransport.hpp"

#include <iostream>
namespace yaodaq
{

class Serial final : public yaodaq::ITransport
{
public:
  YAODAQ_API explicit Serial( const std::string_view name = "default" ) : ITransport( "default", "Serial" ) {}
  YAODAQ_API bool open() final
  {
    m_serial.setPort( getParameters().get_as_f<std::string>( "port" ).or_throw( "Port not provided !" ) );
    m_serial.setBaudrate( getParameters().get_as_f<std::uint32_t>( "baudrate" ).or_throw( "Baudrate not provided" ) );
    m_serial.setBytesize( getParameters().get_as_f<serial_cpp::bytesize_t>( "bytesize" ).or_throw( "bytesize not provided" ) );
    m_serial.setParity( getParameters().get_as_f<serial_cpp::parity_t>( "parity" ).or_throw( "parity not provided" ) );
    m_serial.setStopbits( getParameters().get_as_f<serial_cpp::stopbits_t>( "stopbyte" ).or_throw( "stopbyte not provided" ) );
    m_serial.open();
    return true;
  }

  YAODAQ_API bool close() final
  {
    m_serial.close();
    return true;
  }

  YAODAQ_API bool verifyParameters() final
  {
    debug( "verifyParameters() called" );
    return getParameters().contains( "port" );
  }

  YAODAQ_API void write( std::span<const std::byte> data ) override { m_serial.write( reinterpret_cast<const std::uint8_t*>( data.data() ), data.size() ); }

  YAODAQ_API std::vector<yaodaq::TransportPacket> read() override
  {
    std::vector<yaodaq::TransportPacket> packets;
    std::string                          ret = m_serial.readline();
    if( ret.empty() ) return packets;
    packets.push_back( yaodaq::TransportPacket( std::vector<std::byte>( reinterpret_cast<const std::byte*>( ret.data() ), reinterpret_cast<const std::byte*>( ret.data() ) + ret.size() ), "serial" ) );
    return packets;
  }

private:
  serial_cpp::Serial m_serial;
};

}  // namespace yaodaq
