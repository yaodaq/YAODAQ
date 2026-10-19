#include "yaodaq/Board.hpp"
#include "yaodaq/Connector.hpp"
#include "yaodaq/codec/DefaultCodec.hpp"
#include "yaodaq/transport/Serial.hpp"

#include <CLI/CLI.hpp>
#include <cpp-terminal/color.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <fmt/ranges.h>
#include <simdjson.h>

struct CAENReturn
{
  std::int8_t              board{ -1 };
  std::vector<std::string> values;
};

class CAENHVCodec final : public yaodaq::ICodec
{
public:
  YAODAQ_API CAENHVCodec() : ICodec( "default", "default" ) {}
  YAODAQ_API virtual ~CAENHVCodec() noexcept = default;
  YAODAQ_API virtual std::vector<std::byte> encode( const yaodaq::Message& msg ) const final
  {
    switch( msg.type() )
    {
      case yaodaq::Message::Type::RawData:
      {
        const auto& raw = static_cast<const yaodaq::RawData&>( msg );
        std::string str{ reinterpret_cast<const char*>( raw.payload().data() ), raw.payload().size() };
        return { raw.payload().begin(), raw.payload().end() };
      }
      default:
      {
        error( "Message is on type {} not RawData !", msg.type_str() );
        return {};
      }
    }
  }
  YAODAQ_API virtual std::unique_ptr<yaodaq::Message> decode( const yaodaq::TransportPacket& data ) const final
  {
    std::unique_ptr<yaodaq::RawData> message = std::make_unique<yaodaq::RawData>( data.payload, data.channel );
    std::string                      caen_message{ reinterpret_cast<const char*>( message->payload().data() ), message->payload().size() };
    switch( parseCaenError( caen_message ) )
    {
      case CaenError::Command:
      {
        return yaodaq::RawDataBuilder::build( R"({"error":"Wrong command Format or command not recognized"})", data.channel );
      }
      case CaenError::Channel:
      {
        return yaodaq::RawDataBuilder::build( R"({"error":"Channel Field not present or wrong Channel value"})", data.channel );
      }
      case CaenError::Parameter:
      {
        return yaodaq::RawDataBuilder::build( R"({"error":"Field parameter not present or parameter not recognized"})", data.channel );
      }
      case CaenError::Value:
      {
        return yaodaq::RawDataBuilder::build( R"json({"error":"Wrong set value (<Min or >Max)"})json", data.channel );
      }
      case CaenError::LocalMode:
      {
        return yaodaq::RawDataBuilder::build( R"({"error":"Command SET with module in LOCAL mode"})", data.channel );
      }
      case CaenError::None:
      {
        CAENReturn               result = parseCaenAnswer( caen_message );
        std::vector<std::string> quoted_values;
        for( const auto& value: result.values ) quoted_values.push_back( fmt::format( R"("{}")", value ) );
        std::string json = fmt::format( R"({{"board":{},"values":[{}]}})", result.board, fmt::join( quoted_values, "," ) );
        return yaodaq::RawDataBuilder::build( json, data.channel );
      }
      default: return yaodaq::RawDataBuilder::build( R"({"error":"Should not happen"})", data.channel );
    }
  }

private:
  enum class CaenError
  {
    None,
    Command,
    Channel,
    Parameter,
    Value,
    LocalMode
  };
  CaenError parseCaenError( const std::string_view response ) const noexcept
  {
    if( response.find( ",CMD:ERR" ) != std::string_view::npos ) return CaenError::Command;
    if( response.find( ",CH:ERR" ) != std::string_view::npos ) return CaenError::Channel;
    if( response.find( ",PAR:ERR" ) != std::string_view::npos ) return CaenError::Parameter;
    if( response.find( ",VAL:ERR" ) != std::string_view::npos ) return CaenError::Value;
    if( response.find( ",LOC:ERR" ) != std::string_view::npos ) return CaenError::LocalMode;
    return CaenError::None;
  }
  CAENReturn parseCaenAnswer( const std::string_view response ) const noexcept
  {
    CAENReturn  result;
    std::size_t pos = response.find( "#BD:" );
    if( pos != std::string::npos )
    {
      pos += 4;
      std::size_t end = response.find( ',', pos );
      result.board    = std::stoi( response.substr( pos, end - pos ).data() );
    }

    // Extract any number of VAL values
    pos = response.find( "VAL:" );
    if( pos != std::string::npos )
    {
      pos += 4;
      while( pos <= response.size() )
      {
        std::size_t end   = response.find( ';', pos );
        auto        value = response.substr( pos, end == std::string_view::npos ? std::string_view::npos : end - pos );
        // Remove trailing CR/LF
        while( !value.empty() && ( value.back() == '\n' || value.back() == '\r' ) ) value.remove_suffix( 1 );
        // Remove leading CR/LF, if any
        while( !value.empty() && ( value.front() == '\n' || value.front() == '\r' ) ) value.remove_prefix( 1 );
        result.values.emplace_back( value );
        if( end == std::string_view::npos ) break;
        pos = end + 1;
      }
    }
    return result;
  }
};

class CAENHV : public yaodaq::Board
{
public:
  CAENHV( yaodaq::BoardConfig& cfg, const std::string_view name ) : yaodaq::Board( cfg, name, "CAENHV" )
  {
    // Channel monitoring: voltage
    Add( "getVoltageSetpoint", jsonrpc::GetHandle( &CAENHV::getVoltageSetpoint, *this ) );
    Add( "getVoltageMinimum", jsonrpc::GetHandle( &CAENHV::getVoltageMinimum, *this ) );
    Add( "getVoltageMaximum", jsonrpc::GetHandle( &CAENHV::getVoltageMaximum, *this ) );
    Add( "getVoltageDecimals", jsonrpc::GetHandle( &CAENHV::getVoltageDecimals, *this ) );
    Add( "getMonitoredVoltage", jsonrpc::GetHandle( &CAENHV::getMonitoredVoltage, *this ) );
    Add( "getMaximumVoltageSetpoint", jsonrpc::GetHandle( &CAENHV::getMaximumVoltageSetpoint, *this ) );
    Add( "getMaximumVoltageMinimum", jsonrpc::GetHandle( &CAENHV::getMaximumVoltageMinimum, *this ) );
    Add( "getMaximumVoltageMaximum", jsonrpc::GetHandle( &CAENHV::getMaximumVoltageMaximum, *this ) );
    Add( "getMaximumVoltageDecimals", jsonrpc::GetHandle( &CAENHV::getMaximumVoltageDecimals, *this ) );

    // Channel monitoring: current
    Add( "getCurrentSetpoint", jsonrpc::GetHandle( &CAENHV::getCurrentSetpoint, *this ) );
    Add( "getCurrentMinimum", jsonrpc::GetHandle( &CAENHV::getCurrentMinimum, *this ) );
    Add( "getCurrentMaximum", jsonrpc::GetHandle( &CAENHV::getCurrentMaximum, *this ) );
    Add( "getCurrentSetpointDecimals", jsonrpc::GetHandle( &CAENHV::getCurrentSetpointDecimals, *this ) );
    Add( "getMonitoredCurrent", jsonrpc::GetHandle( &CAENHV::getMonitoredCurrent, *this ) );
    Add( "getCurrentMonitorRange", jsonrpc::GetHandle( &CAENHV::getCurrentMonitorRange, *this ) );
    Add( "getCurrentMonitorDecimals", jsonrpc::GetHandle( &CAENHV::getCurrentMonitorDecimals, *this ) );

    // Channel monitoring: ramp rates
    Add( "getRampUpRate", jsonrpc::GetHandle( &CAENHV::getRampUpRate, *this ) );
    Add( "getRampUpRateMinimum", jsonrpc::GetHandle( &CAENHV::getRampUpRateMinimum, *this ) );
    Add( "getRampUpRateMaximum", jsonrpc::GetHandle( &CAENHV::getRampUpRateMaximum, *this ) );
    Add( "getRampUpRateDecimals", jsonrpc::GetHandle( &CAENHV::getRampUpRateDecimals, *this ) );
    Add( "getRampDownRate", jsonrpc::GetHandle( &CAENHV::getRampDownRate, *this ) );
    Add( "getRampDownRateMinimum", jsonrpc::GetHandle( &CAENHV::getRampDownRateMinimum, *this ) );
    Add( "getRampDownRateMaximum", jsonrpc::GetHandle( &CAENHV::getRampDownRateMaximum, *this ) );
    Add( "getRampDownRateDecimals", jsonrpc::GetHandle( &CAENHV::getRampDownRateDecimals, *this ) );

    // Channel monitoring: trip and protection
    Add( "getTripTime", jsonrpc::GetHandle( &CAENHV::getTripTime, *this ) );
    Add( "getTripTimeMinimum", jsonrpc::GetHandle( &CAENHV::getTripTimeMinimum, *this ) );
    Add( "getTripTimeMaximum", jsonrpc::GetHandle( &CAENHV::getTripTimeMaximum, *this ) );
    Add( "getTripTimeDecimals", jsonrpc::GetHandle( &CAENHV::getTripTimeDecimals, *this ) );
    Add( "getPowerDownMode", jsonrpc::GetHandle( &CAENHV::getPowerDownMode, *this ) );
    Add( "getPolarity", jsonrpc::GetHandle( &CAENHV::getPolarity, *this ) );
    Add( "getStatus", jsonrpc::GetHandle( &CAENHV::getStatus, *this ) );
    Add( "getZeroCurrentDetectionStatus", jsonrpc::GetHandle( &CAENHV::getZeroCurrentDetectionStatus, *this ) );
    Add( "getZeroCurrentAdjustMode", jsonrpc::GetHandle( &CAENHV::getZeroCurrentAdjustMode, *this ) );

    // Board monitoring
    Add( "getBoardName", jsonrpc::GetHandle( &CAENHV::getBoardName, *this ) );
    Add( "getChannelCount", jsonrpc::GetHandle( &CAENHV::getChannelCount, *this ) );
    Add( "getFirmwareRelease", jsonrpc::GetHandle( &CAENHV::getFirmwareRelease, *this ) );
    Add( "getSerialNumber", jsonrpc::GetHandle( &CAENHV::getSerialNumber, *this ) );
    Add( "getInterlockStatus", jsonrpc::GetHandle( &CAENHV::getInterlockStatus, *this ) );
    Add( "getInterlockMode", jsonrpc::GetHandle( &CAENHV::getInterlockMode, *this ) );
    Add( "getControlMode", jsonrpc::GetHandle( &CAENHV::getControlMode, *this ) );
    Add( "getBusTerminationStatus", jsonrpc::GetHandle( &CAENHV::getBusTerminationStatus, *this ) );
    Add( "getAlarmStatus", jsonrpc::GetHandle( &CAENHV::getAlarmStatus, *this ) );

    // Channel configuration and control
    Add( "setVoltageSetpoint", jsonrpc::GetHandle( &CAENHV::setVoltageSetpoint, *this ) );
    Add( "setCurrentSetpoint", jsonrpc::GetHandle( &CAENHV::setCurrentSetpoint, *this ) );
    Add( "setMaximumVoltageSetpoint", jsonrpc::GetHandle( &CAENHV::setMaximumVoltageSetpoint, *this ) );
    Add( "setRampUpRate", jsonrpc::GetHandle( &CAENHV::setRampUpRate, *this ) );
    Add( "setRampDownRate", jsonrpc::GetHandle( &CAENHV::setRampDownRate, *this ) );
    Add( "setTripTime", jsonrpc::GetHandle( &CAENHV::setTripTime, *this ) );
    Add( "setPowerDownMode", jsonrpc::GetHandle( &CAENHV::setPowerDownMode, *this ) );
    Add( "setCurrentMonitorRange", jsonrpc::GetHandle( &CAENHV::setCurrentMonitorRange, *this ) );
    Add( "turnOn", jsonrpc::GetHandle( &CAENHV::turnOn, *this ) );
    Add( "turnOff", jsonrpc::GetHandle( &CAENHV::turnOff, *this ) );
    Add( "setZeroCurrentAdjustmentEnabled", jsonrpc::GetHandle( &CAENHV::setZeroCurrentAdjustmentEnabled, *this ) );
    Add( "setZeroCurrentAdjustmentDisables", jsonrpc::GetHandle( &CAENHV::setZeroCurrentAdjustmentDisables, *this ) );

    // Board configuration and control
    Add( "setInterlockMode", jsonrpc::GetHandle( &CAENHV::setInterlockMode, *this ) );
    Add( "clearAlarm", jsonrpc::GetHandle( &CAENHV::clearAlarm, *this ) );
  }
  ~CAENHV() final {}
  double getVoltageSetpoint( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "VSET" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getVoltageMinimum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "VMIN" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getVoltageMaximum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "VMAX" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  std::size_t getVoltageDecimals( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "VDEC" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getMonitoredVoltage( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "VMON" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }

  double getCurrentSetpoint( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "ISET" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }

  double getCurrentMinimum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "IMIN" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getCurrentMaximum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "IMAX" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }

  std::size_t getCurrentSetpointDecimals( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "ISDEC" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }

  double getMonitoredCurrent( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "IMON" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }

  std::string getCurrentMonitorRange( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "IMRANGE" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0];
  }

  std::size_t getCurrentMonitorDecimals( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "IMDEC" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }

  double getMaximumVoltageSetpoint( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "MAXV" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }

  double getMaximumVoltageMinimum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "MVMIN" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }

  double getMaximumVoltageMaximum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "MVMAX" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }

  std::size_t getMaximumVoltageDecimals( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "MVDEC" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }

  double getRampUpRate( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "RUP" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getRampUpRateMinimum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "RUPMIN" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getRampUpRateMaximum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "RUPMAX" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  std::size_t getRampUpRateDecimals( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "RUPDEC" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getRampDownRate( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "RDW" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getRampDownRateMinimum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "RDWMIN" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getRampDownRateMaximum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "RDWMAX" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  std::size_t getRampDownRateDecimals( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "RDWDEC" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getTripTime( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "TRIP" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getTripTimeMinimum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "TRIPMIN" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  double getTripTimeMaximum( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "TRIPMAX" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  std::size_t getTripTimeDecimals( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "TRIPDEC" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  std::string getPowerDownMode( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "PDWN" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0];
  }
  char getPolarity( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "POL" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0][0];
  }
  std::string getStatus( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "STAT" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0];
  }
  bool getZeroCurrentDetectionStatus( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "ZCDTC" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    std::string ret = parseReturnValue( response.get() ).values[0];
    if( ret == "ON" ) return true;
    else
      return false;
  }
  bool getZeroCurrentAdjustMode( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, channel, "ZCADJ" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    std::string ret = parseReturnValue( response.get() ).values[0];
    if( ret == "EN" ) return true;
    else
      return false;
  }

  // Module monitor

  std::string getBoardName( const int board )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, "BDNAME" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0];
  }
  std::size_t getChannelCount( const int board )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, "BDNCH" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return std::stod( parseReturnValue( response.get() ).values[0] );
  }
  std::string getFirmwareRelease( const int board )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, "BDFREL" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0];
  }
  std::string getSerialNumber( const int board )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, "BDSNUM" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0];
  }
  std::string getInterlockStatus( const int board )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, "BDILK" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0];
  }
  std::string getInterlockMode( const int board )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, "BDILKM" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0];
  }
  std::string getControlMode( const int board )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, "BDCTR" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0];
  }
  bool getBusTerminationStatus( const int board )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, "BDTERM" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    std::string ret = parseReturnValue( response.get() ).values[0];
    if( ret == "ON" ) return true;
    else
      return false;
  }
  std::string getAlarmStatus( const int board )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_get_command( board, "BDALARM" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
    return parseReturnValue( response.get() ).values[0];
  }

  // Set Channels
  void setVoltageSetpoint( const int board, const int channel, const double& val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "VSET", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void setCurrentSetpoint( const int board, const int channel, const double& val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "ISET", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void setMaximumVoltageSetpoint( const int board, const int channel, const double& val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "MAXV", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void setRampUpRate( const int board, const int channel, const double& val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "RUP", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void setRampDownRate( const int board, const int channel, const double& val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "RDWN", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void setTripTime( const int board, const int channel, const double& val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "TRIP", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void setPowerDownMode( const int board, const int channel, const double& val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "PDWN", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void setCurrentMonitorRange( const int board, const int channel, const double& val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "IMRANGE", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void turnOn( const int board, const int channel, const double& val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "ON", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void turnOff( const int board, const int channel, const double& val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "OFF", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void setZeroCurrentAdjustmentEnabled( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "ZCADJ", "EN" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void setZeroCurrentAdjustmentDisables( const int board, const int channel )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, channel, "ZCADJ", "DIS" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }

  // Set Board
  void setInterlockMode( const int board, const std::string_view val )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, "BDILKM", val ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }
  void clearAlarm( const int board )
  {
    auto response = request_fifo( std::make_unique<yaodaq::RawData>( yaodaq::RawDataBuilder::from_text( create_set_command( board, "DCLR" ), "" ) ) );
    if( response.wait_for( std::chrono::seconds( 3 ) ) != std::future_status::ready ) throw std::runtime_error( "CAEN response timeout" );
  }

private:
  std::string create_get_command( const std::uint8_t module, const std::uint8_t channel, const std::string_view param ) { return std::move( fmt::format( "$BD:{:02},CMD:MON,CH:{},PAR:{}\r\n", module, channel, param ) ); }
  std::string create_set_command( const std::uint8_t module, const std::uint8_t channel, const std::string_view param, const double& value ) { return std::move( fmt::format( "$BD:{:02},CMD:SET,CH:{},PAR:{},VAL:{}\r\n", module, channel, param, value ) ); }
  std::string create_set_command( const std::uint8_t module, const std::uint8_t channel, const std::string_view param, const std::string_view value )
  { return std::move( fmt::format( "$BD:{:02},CMD:SET,CH:{},PAR:{},VAL:{}\r\n", module, channel, param, value ) ); }
  std::string create_get_command( const std::uint8_t module, const std::string_view param ) { return std::move( fmt::format( "$BD:{:02},CMD:MON,PAR:{}\r\n", module, param ) ); }
  std::string create_set_command( const std::uint8_t module, const std::string_view param, const double& value ) { return std::move( fmt::format( "$BD:{:02},CMD:SET,PAR:{},VAL:{}\r\n", module, param, value ) ); }
  std::string create_set_command( const std::uint8_t module, const std::string_view param, const std::string_view value ) { return std::move( fmt::format( "$BD:{:02},CMD:SET,PAR:{},VAL:{}\r\n", module, param, value ) ); }
  std::string create_set_command( const std::uint8_t module, const std::string_view param ) { return std::move( fmt::format( "$BD:{:02},CMD:SET,PAR:{}\r\n", module, param ) ); }

  CAENReturn parseReturnValue( std::unique_ptr<yaodaq::Message> message )
  {
    std::unique_ptr<yaodaq::RawData> rawData;
    if( auto* ptr = dynamic_cast<yaodaq::RawData*>( message.get() ) )
    {
      message.release();
      rawData.reset( ptr );
    }
    std::string                str( reinterpret_cast<const char*>( rawData->payload().data() ), rawData->payload().size() );
    simdjson::ondemand::parser parser;
    simdjson::padded_string    padded_json( str );
    auto                       doc = parser.iterate( padded_json );

    auto error_result = doc.find_field( "error" );
    if( error_result.error() == simdjson::SUCCESS )
    {
      std::string_view error_message;
      if( error_result.get_string().get( error_message ) == simdjson::SUCCESS ) throw yaodaq::Exception( error_message );
      else
        throw yaodaq::Exception( "error parsing CAENHV return message" );
    }

    CAENReturn ret;
    auto       board_result = doc["board"].get_int64();
    if( board_result.error() == simdjson::SUCCESS ) { ret.board = static_cast<int>( board_result.value() ); }
    auto array_result = doc["values"].get_array();
    if( array_result.error() == simdjson::SUCCESS )
    {
      for( auto value: array_result.value() )
      {
        std::string v;
        if( value.get_string().get( v ) == simdjson::SUCCESS ) ret.values.push_back( v );
      }
    }
    return ret;
  }
};

int main( int argc, char* argv[] )
{
  Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
  CLI::App app{ "DCT" };
  argv = app.ensure_utf8( argv );
  std::string name{ "R1470ETD" };
  app.add_option( "-n,--name", name, "Name of the client" );
  std::string host{ "127.0.0.1" };
  app.add_option( "-i,--ip", host, "IP of the server" ) /*->check( CLI::ValidIPV4 )*/;
  int port{ 8888 };
  app.add_option( "-p,--port", port, "Port to listen" )->check( CLI::Range( 0, 65535 ) );
  yaodaq::verbosity::level verbosity{ yaodaq::verbosity::level::info };
  app.add_option( "--verbosity", verbosity, "Verbosity" )->transform( CLI::CheckedTransformer( yaodaq::verbosity::map, CLI::ignore_case ) );
  std::string board_port{ "/dev/ttyACM0" };
  app.add_option( "--board_port", board_port, "port of the board" );
  std::uint32_t baudrate{ 9600 };
  app.add_option( "--baudrate", baudrate, "baudrate" );
  serial_cpp::bytesize_t bytesize{ serial_cpp::bytesize_t::eightbits };
  app.add_option( "--bytesize", bytesize, "bytesize" );
  serial_cpp::parity_t parity{ serial_cpp::parity_t::parity_none };
  app.add_option( "--parity", parity, "parity" );
  serial_cpp::stopbits_t stopbits{ serial_cpp::stopbits_t::stopbits_one };
  app.add_option( "--stopbits", stopbits, "stopbits" );

  try
  {
    app.parse( argc, argv );
  }
  catch( const CLI::ParseError& e )
  {
    return app.exit( e );
  }

  yaodaq::BoardConfig cfg( std::make_unique<yaodaq::Connector>( std::make_unique<yaodaq::Serial>( "CAEN" ), std::make_unique<CAENHVCodec>() ) );

  cfg.transportParameters().set( "port", board_port ).set( "baudrate", baudrate ).set( "bytesize", bytesize ).set( "parity", parity ).set( "stopbyte", stopbits );
  cfg.setPort( port ).setHost( host ).verbosity( verbosity );
  CAENHV board( cfg, name );
  board.link();
  //board.dispatcher().subscribe<yaodaq::RawData>(
  //  [&board]( const yaodaq::RawData& msg )
  //  {
  //
  //    std::string_view text( reinterpret_cast<const char*>( msg.payload().data() ), msg.payload().size() );
  //    if(!text.empty())board.info(text);
  //  } );

  std::size_t nbrCTLC{ 3 };
  Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press " << std::to_string( nbrCTLC ) << " times CTRL+Q to stop" << Term::color_fg( Term::Color::Name::Default ) << std::endl;

  while( true )
  {
    Term::Event event = Term::read_event();

    switch( auto key = event.type() )
    {
      case Term::Event::Type::Key:
      {
        Term::Key key( event );

        if( key == Term::Key::Ctrl_Q )
        {
          --nbrCTLC;

          if( nbrCTLC == 0 ) return 0;

          Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press Ctrl+Q " << std::to_string( nbrCTLC ) << " times to quit" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
        }
        else if( key == Term::Key::a )
        {
          std::cout << board.getVoltageSetpoint( 0, 1 ) << std::endl;
          board.setVoltageSetpoint( 0, 1, 2000.0 );
          std::cout << board.getVoltageSetpoint( 0, 1 ) << std::endl;
          std::cout << board.getAlarmStatus( 0 ) << std::endl;
        }
        else
        {
          nbrCTLC = 3;
        }

        break;
      }

      default: break;
    }
  }
  return 0;
}
