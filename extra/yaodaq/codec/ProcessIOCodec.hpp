#pragma once
#include "fmt/format.h"
#include "yaodaq/Exception.hpp"
#include "yaodaq/ICodec.hpp"
#include "yaodaq/Message.hpp"

#include <atomic>
#include <mutex>
#include <string>

namespace yaodaq
{

class ProcessIOCodec : public ICodec
{
public:
  ProcessIOCodec( const std::string_view name, const std::string_view command = {}, const std::string_view prologue = "__YAODAQ_PROLOGUE_UUID{}__", const std::string_view epilogue = "__YAODAQ_EPILOGUE_UUID{}__" ) :
    ICodec( name, "ProcessIOCodec" ), m_command( command )
  {
    if( m_command.find( "{}" ) == std::string::npos ) throw Exception( "command must contains {}" );
    if( !prologue.empty() )
    {
      if( epilogue.empty() ) throw Exception( "prologue without epilogue is useless" );
      auto found = prologue.find( "{}" );
      if( found == std::string::npos ) throw Exception( "prologue should contains {}" );
      m_prologue = std::pair<std::string, std::string>( prologue.substr( 0, found ), prologue.substr( found + 2 ) );
    }
    if( !epilogue.empty() )
    {
      auto found = epilogue.find( "{}" );
      if( found == std::string::npos ) throw Exception( "epilogue should contains {}" );
      m_epilogue = std::pair<std::string, std::string>( epilogue.substr( 0, found ), epilogue.substr( found + 2 ) );
    }
  }

  std::vector<std::byte> encode( const Message& msg ) const override
  {
    switch( msg.type() )
    {
      case yaodaq::Message::Type::RawData:
      {
        const auto& raw = static_cast<const RawData&>( msg );
        return WrapAndSend( raw );
        break;
      }
      default:
      {
        error( "Message is on type {} not RawData !", msg.type_str() );
        return {};
      }
    }
  }
  void reset()
  {
    std::lock_guard lock( m_mutex );
    m_buffer.clear();
  }

  std::unique_ptr<Message> decode( const TransportPacket& data ) const override
  {
    // No framing configured
    if( !has_prologue() && !has_epilogue() ) return std::make_unique<RawData>( data.payload, data.channel );
    // stderr is immediate
    if( data.channel == "stderr" )
    {
      auto raw = std::make_unique<RawData>( data.payload, data.channel, *m_correlationId );
      return raw;
    }
    std::lock_guard lock( m_mutex );
    // accumulate stdout
    m_buffer.append( reinterpret_cast<const char*>( data.payload.data() ), data.payload.size() );
    // Find prologue
    auto prologueBegin = m_buffer.find( m_prologue.first );
    if( prologueBegin == std::string::npos ) return nullptr;
    auto uuidBegin = prologueBegin + m_prologue.first.size();
    auto uuidEnd   = m_buffer.find( m_prologue.second, uuidBegin );
    if( uuidEnd == std::string::npos ) return nullptr;
    std::string uuid          = m_buffer.substr( uuidBegin, uuidEnd - uuidBegin );
    auto        payloadBegin  = uuidEnd + m_prologue.second.size();
    // Find epilogue
    auto        epilogueBegin = m_buffer.find( m_epilogue.first, payloadBegin );
    if( epilogueBegin == std::string::npos ) return nullptr;
    auto epUuidBegin = epilogueBegin + m_epilogue.first.size();
    auto epUuidEnd   = m_buffer.find( m_epilogue.second, epUuidBegin );
    if( epUuidEnd == std::string::npos ) return nullptr;
    std::string epUuid = m_buffer.substr( epUuidBegin, epUuidEnd - epUuidBegin );
    if( epUuid != uuid )
    {
      error( "Prologue/Epilogue UUID mismatch" );
      m_buffer.clear();
      m_correlationId.reset();
      return nullptr;
    }
    // Remember UUID while command is active
    m_correlationId     = uuid;
    std::string payload = m_buffer.substr( payloadBegin, epilogueBegin - payloadBegin );
    auto        raw     = std::make_unique<RawData>( std::span<const std::byte>( reinterpret_cast<const std::byte*>( payload.data() ), payload.size() ), data.channel, uuid );
    // Remove processed frame
    m_buffer.erase( 0, epUuidEnd + m_epilogue.second.size() );
    // Command finished
    m_correlationId.reset();
    return raw;
  }

private:
  bool                   has_epilogue() const noexcept { return !m_epilogue.first.empty() || !m_epilogue.second.empty(); }
  bool                   has_prologue() const noexcept { return !m_prologue.first.empty() || !m_prologue.second.empty(); }
  std::vector<std::byte> WrapAndSend( const RawData& raw_data ) const
  {
    if( m_prologue.first.empty() && m_prologue.second.empty() && m_epilogue.first.empty() && m_epilogue.second.empty() ) return { raw_data.payload().begin(), raw_data.payload().end() };
    std::vector<std::byte> result;
    std::string            prologue;
    prologue = fmt::format( fmt::runtime( m_command ), fmt::format( "{}{}{}", m_prologue.first, raw_data.uuid(), m_prologue.second ) );
    std::string epilogue;
    epilogue = fmt::format( fmt::runtime( m_command ), fmt::format( "{}{}{}", m_epilogue.first, raw_data.uuid(), m_epilogue.second ) );
    result.reserve( prologue.size() + raw_data.payload().size() + epilogue.size() );
    result.insert( result.end(), reinterpret_cast<const std::byte*>( prologue.data() ), reinterpret_cast<const std::byte*>( prologue.data() + prologue.size() ) );
    result.insert( result.end(), raw_data.payload().begin(), raw_data.payload().end() );
    result.insert( result.end(), reinterpret_cast<const std::byte*>( epilogue.data() ), reinterpret_cast<const std::byte*>( epilogue.data() + epilogue.size() ) );
    return result;
  }
  std::string                         m_command;
  std::pair<std::string, std::string> m_prologue;
  std::pair<std::string, std::string> m_epilogue;
  mutable std::string                 m_buffer;
  mutable std::mutex                  m_mutex;
  mutable std::optional<std::string>  m_correlationId;
};

}  // namespace yaodaq
