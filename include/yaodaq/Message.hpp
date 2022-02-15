#ifndef YAODAQ_MESSAGE
#define YAODAQ_MESSAGE

/**
\copyright Copyright 2022 flagarde
*/

#include "nlohmann/json.hpp"
#include "yaodaq/MessageType.hpp"

#include <string>

namespace yaodaq
{

class Identifier;
class Exception;

class Message
{
public:
  Message();
  explicit Message( const nlohmann::json& content, const MessageType& messageType = MessageType::Unknown );
  explicit Message( const std::string& content, const MessageType& messageType = MessageType::Unknown );
  explicit Message( const char* content, const MessageType& messageType = MessageType::Unknown );
  std::string    dump( const int& indent = -1, const char& indent_char = ' ', const bool& ensure_ascii = false, const nlohmann::detail::error_handler_t& error_handler = nlohmann::detail::error_handler_t::strict ) const;
  nlohmann::json get() const;
  nlohmann::json getContent() const;
  std::string    getTypeName() const;
  MessageType    getTypeValue() const;
  std::string    getTimestamp() const;
  std::time_t    getTime() const;
  Identifier     getIdentifier() const;
  void           setFrom( const Identifier& );

protected:
  explicit Message( const MessageType& messageType );
  void           setContent( const nlohmann::json& content );
  void           setContent( const std::string& content );
  void           setContent( const char* content );
  nlohmann::json m_JSON;
};

class MessageException : public Message
{
public:
  explicit MessageException( const Exception& content );
  std::int_least32_t getCode();
  std::string        getDescription();
  std::int_least32_t getLine();
  std::int_least32_t getColumn();
  std::string        getFileName();
  std::string        getFunctionName();
};

}  // namespace yaodaq

#endif  // YAODAQ_MESSAGE
