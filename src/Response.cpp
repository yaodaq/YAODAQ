#include "yaodaq/Response.hpp"

#include "yaodaq/Formatter.hpp"

#include <algorithm>
#include <cpp-terminal/screen.hpp>
#include <fmt/color.h>
#include <simdjson.h>

namespace yaodaq
{

namespace
{

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

// Repeat string `s` `n` times
std::string repeat( const std::string_view s, const std::size_t n )
{
  std::string out;
  out.reserve( s.size() * n );
  for( std::size_t i = 0; i < n; ++i ) out += s;
  return out;
}

// Calculate visible width ignoring ANSI color codes
std::size_t visible_width( const std::string_view s )
{
  std::size_t width  = 0;
  bool        escape = false;
  for( const char c: s )
  {
    if( c == '\033' )
    {
      escape = true;
      continue;
    }
    if( escape )
    {
      if( c == 'm' ) escape = false;
      continue;
    }
    ++width;
  }
  return width;
}

// Pad string to width
std::string pad( const std::string_view s, const std::size_t width )
{
  std::size_t len = visible_width( s );
  if( len >= width ) return s.data();
  return std::string( s ) + std::string( width - len, ' ' );
}

// Truncate string to width
std::string trunc( const std::string_view s, const std::size_t width )
{
  if( width <= 0 ) return {};
  if( visible_width( s ) <= width ) return s.data();
  if( width == 1 ) return "…";
  return std::string( s.substr( 0, width - 1 ) ) + "…";
}

// Draw table borders
std::string border2( const std::string_view left, const std::string_view mid, const std::string_view right, const std::size_t w1, const std::size_t w2 ) { return fmt::format( "{}{}{}{}{}\n", left, repeat( "─", w1 + 2 ), mid, repeat( "─", w2 + 2 ), right ); }

std::string border3( const std::string_view left, const std::string_view mid, const std::string_view right, const std::size_t w1, const std::size_t w2, const std::size_t w3 )
{ return fmt::format( "{}{}{}{}{}{}{}\n", left, repeat( "─", w1 + 2 ), mid, repeat( "─", w2 + 2 ), mid, repeat( "─", w3 + 2 ), right ); }

// Convert node identifier to string
std::string node_id( const simdjson::dom::element& item )
{
  std::string component = "?";
  std::string type      = "?";
  std::string name      = "?";
  const auto  id        = item["yaodaq_id"];
  if( !id.error() )
  {
    if( !id["component"].error() ) component = std::string( id["component"].get_string().value_unsafe() );
    if( !id["type"].error() ) type = std::string( id["type"].get_string().value_unsafe() );
    if( !id["name"].error() ) name = std::string( id["name"].get_string().value_unsafe() );
  }
  return fmt::format( R"({}\{}\{})", component, type, name );
}

// Convert JSON element to single table cell without minify
std::string table_cell( const simdjson::dom::element& e, const size_t indent = 0 )
{
  std::string out;
  switch( e.type() )
  {
    case simdjson::dom::element_type::OBJECT:
    {
      const simdjson::dom::object obj = e;
      out += "{";
      bool first = true;
      for( const auto [k, v]: obj )
      {
        if( !first ) out += ", ";
        else
          first = false;
        out += fmt::format( "\"{}\": {}", std::string_view( k ), table_cell( v, indent + 1 ) );
      }
      out += "}";
      break;
    }
    case simdjson::dom::element_type::ARRAY:
    {
      const simdjson::dom::array arr = e;
      out += "[";
      bool first = true;
      for( const auto v: arr )
      {
        if( !first ) out += ", ";
        else
          first = false;
        out += table_cell( v, indent + 1 );
      }
      out += "]";
      break;
    }
    case simdjson::dom::element_type::STRING: out += string_color( e.get_string().value_unsafe() ); break;
    case simdjson::dom::element_type::INT64: out += number_int_color( e.get_int64() ); break;
    case simdjson::dom::element_type::UINT64: out += number_int_color( static_cast<long long>( e.get_uint64() ) ); break;
    case simdjson::dom::element_type::DOUBLE: out += number_float_color( e.get_double() ); break;
    case simdjson::dom::element_type::BOOL: out += bool_color( e.get_bool() ); break;
    case simdjson::dom::element_type::NULL_VALUE: out += null_color(); break;
    default: out += string_color( "[unsupported]" ); break;
  }
  return out;
}

// Color helper for table nodes
auto blue = []( const std::string_view s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::cornflower_blue ) ) ); };

}  // namespace

// ============================================================
// PRETTY FORMAT
// ============================================================

std::string Response::pretty_format()
{
  static thread_local simdjson::dom::parser parser;
  simdjson::dom::element                    root;
  if( parser.parse( m_raw ).get( root ) != simdjson::SUCCESS ) { return fmt::format( fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold, "Invalid JSON response\n" ); }
  return Formatter::format( root );
}

// ============================================================
// TABULATE
// ============================================================

std::string Response::tabulate()
{
  static thread_local simdjson::dom::parser parser;
  simdjson::dom::element                    root;

  if( parser.parse( m_raw ).get( root ) != simdjson::SUCCESS ) { return fmt::format( fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold, "Invalid JSON-RPC response\n" ); }

  if( root.type() != simdjson::dom::element_type::ARRAY ) { return fmt::format( fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold, "Expected JSON-RPC batch array\n" ); }

  const std::size_t width    = std::max( static_cast<std::size_t>( 80 ), static_cast<std::size_t>( Term::screen_size().columns() ) );
  const std::size_t NODE_W   = std::max( static_cast<std::size_t>( 30 ), static_cast<std::size_t>( width / 3 ) );
  const std::size_t RESULT_W = std::max( static_cast<std::size_t>( 20 ), static_cast<std::size_t>( width - NODE_W - 9 ) );
  const std::size_t CODE_W{ 10 };
  const std::size_t MESSAGE_W = std::max( static_cast<std::size_t>( 20 ), static_cast<std::size_t>( width - NODE_W - CODE_W - 12 ) );

  std::string ok;
  std::string err;

  // SUCCESS TABLE
  ok += fmt::format( "{}\n", fmt::styled( "Results", fmt::fg( fmt::color::green ) | fmt::emphasis::bold ) );
  ok += border2( "┌", "┬", "┐", NODE_W, RESULT_W );
  ok += fmt::format( "│ {:<{}} │ {:<{}} │\n", "Client", NODE_W, "Result", RESULT_W );
  ok += border2( "├", "┼", "┤", NODE_W, RESULT_W );

  // ERROR TABLE
  err += fmt::format( "{}\n", fmt::styled( "Errors", fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold ) );
  err += border3( "┌", "┬", "┐", NODE_W, CODE_W, MESSAGE_W );
  err += fmt::format( "│ {:<{}} │ {:<{}} │ {:<{}} │\n", "NODE", NODE_W, "CODE", CODE_W, "MESSAGE", MESSAGE_W );
  err += border3( "├", "┼", "┤", NODE_W, CODE_W, MESSAGE_W );

  for( const auto item: simdjson::dom::array( root ) )
  {
    const std::string node = trunc( node_id( item ), NODE_W );
    const auto        rpc  = item["result"];
    if( rpc.error() ) continue;
    // ERROR
    const auto error = rpc["error"];
    if( !error.error() )
    {
      std::string code       = "?";
      std::string message    = "?";
      auto        code_field = error["code"];
      if( !code_field.error() ) code = table_cell( code_field.value() );
      auto msg_field = error["message"];
      if( !msg_field.error() ) message = table_cell( msg_field.value() );
      err += fmt::format( "│ {} │ {} │ {} │\n", blue( pad( node, NODE_W ) ), pad( trunc( code, CODE_W ), CODE_W ), pad( trunc( message, MESSAGE_W ), MESSAGE_W ) );
      continue;
    }
    // SUCCESS
    std::string value        = "?";
    auto        result_field = rpc["result"];
    if( !result_field.error() ) value = table_cell( result_field.value() );
    ok += fmt::format( "│ {} │ {} │\n", blue( pad( node, NODE_W ) ), pad( trunc( value, RESULT_W ), RESULT_W ) );
  }

  ok += border2( "└", "┴", "┘", NODE_W, RESULT_W );
  err += border3( "└", "┴", "┘", NODE_W, CODE_W, MESSAGE_W );

  return ok + "\n\n" + err;
}

}  // namespace yaodaq
