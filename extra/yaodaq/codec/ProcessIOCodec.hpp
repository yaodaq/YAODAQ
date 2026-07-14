#pragma once
#include <atomic>
#include <string>
#include <mutex>
#include "yaodaq/ICodec.hpp"
#include "yaodaq/Message.hpp"
#include "fmt/format.h"
#include "yaodaq/Exception.hpp"

namespace yaodaq
{

class ProcessIOCodec : public ICodec
{
public:
  ProcessIOCodec(const std::string_view name,const std::string_view command = {}, const std::string_view prologue = "__YAODAQ_PROLOGUE_UUID{}__", const std::string_view epilogue = "__YAODAQ_EPILOGUE_UUID{}__" ): ICodec(name, "ProcessIOCodec"),
  m_command(command)
  {
    if(m_command.find("{}")==std::string::npos) throw Exception("command must contains {}");
    if(!prologue.empty())
    {
      if(epilogue.empty()) throw Exception("prologue without epilogue is useless");
      auto found = prologue.find("{}");
      if(found == std::string::npos ) throw Exception("prologue should contains {}");
      m_prologue = std::pair<std::string,std::string>(prologue.substr(0, found),prologue.substr(found+2));
    }
    if(!epilogue.empty())
    {
      auto found = epilogue.find("{}");
      if(found == std::string::npos) throw Exception("epilogue should contains {}");
      m_epilogue = std::pair<std::string,std::string>(epilogue.substr(0, found),epilogue.substr(found+2));
    } 
  }

  std::vector<std::byte> encode(const Message& msg) const override
  {
    switch(msg.type())
    {
      case yaodaq::Message::Type::RawData:
      {
        const auto& raw = static_cast<const RawData&>(msg);
        return WrapAndSend(raw);
        break;
      }
      default:
      {
        error("Message is on type {} not RawData !",msg.type_str());
        return {};
      }
    }
  }
void reset()
{
    std::lock_guard lock(m_mutex);
    m_buffer.clear();
}
std::unique_ptr<Message> decode(const TransportPacket& data) const override
{
  // No framing
  if(!has_epilogue() && !has_prologue()) return std::make_unique<RawData>(data.payload,data.channel);
  if(data.channel == "stderr") return std::make_unique<RawData>(data.payload,data.channel); // error should be availble NOW !!!
  
  std::string incoming(reinterpret_cast<const char*>(data.payload.data()),data.payload.size());
  // Need epilogue to know where the message ends
  { 
    std::lock_guard lock(m_mutex);
    m_buffer += incoming;
  }
  auto epilogue_start = m_buffer.find(m_epilogue.first);
  if(epilogue_start == std::string::npos)
  {
    return nullptr;
  }
  else
  {
    std::lock_guard lock(m_mutex);
    std::unique_ptr<RawData> ret = std::make_unique<RawData>(std::span<const std::byte>(reinterpret_cast<const std::byte*>(m_buffer.data()),m_buffer.size()),data.channel);
    m_buffer.clear();
      return ret;
    }
  return nullptr;
}

private:
  bool has_epilogue() const noexcept
  {
    return !m_epilogue.first.empty() || !m_epilogue.second.empty();
  }
  bool has_prologue() const noexcept
  {
    return !m_prologue.first.empty() || !m_prologue.second.empty();
  }
  std::vector<std::byte> WrapAndSend(const RawData& raw_data) const
  {
    if(m_prologue.first.empty() && m_prologue.second.empty() && m_epilogue.first.empty() && m_epilogue.second.empty()) return  {
            raw_data.payload().begin(),
            raw_data.payload().end()
        };
    std::vector<std::byte> result;
    std::string prologue;
    prologue = fmt::format(fmt::runtime(m_command),fmt::format("{}{}{}",m_prologue.first, raw_data.uuid(),m_prologue.second));
    std::string epilogue;
    epilogue = fmt::format(fmt::runtime(m_command),fmt::format("{}{}{}",m_epilogue.first, raw_data.uuid(),m_epilogue.second));
    result.reserve(prologue.size()+raw_data.payload().size()+epilogue.size());
    result.insert(result.end(),reinterpret_cast<const std::byte*>(prologue.data()),reinterpret_cast<const std::byte*>(prologue.data() + prologue.size()));
    result.insert(result.end(),raw_data.payload().begin(),raw_data.payload().end());
    result.insert(result.end(),reinterpret_cast<const std::byte*>(epilogue.data()),reinterpret_cast<const std::byte*>(epilogue.data() + epilogue.size()));
    return result;
  }
  std::string m_command;
  std::pair<std::string,std::string> m_prologue;
  std::pair<std::string,std::string> m_epilogue;
  mutable std::string m_buffer;
  mutable std::mutex m_mutex;
};

} // namespace yaodaq