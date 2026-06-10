#include "yaodaq/Response.hpp"

#include <algorithm>
#include <cpp-terminal/screen.hpp>
#include <fmt/color.h>
#include <simdjson.h>

namespace yaodaq
{

namespace
{

// ============================================================
// UTF8 HELPERS
// ============================================================

std::string repeat( std::string_view s, std::size_t n )
{
  std::string out;
  out.reserve( s.size() * n );

  for( std::size_t i = 0; i < n; ++i ) out += s;

  return out;
}

// ============================================================
// ANSI WIDTH
// ============================================================

int visible_width( const std::string& s )
{
  int  width  = 0;
  bool escape = false;

  for( char c: s )
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

std::string pad( const std::string& s, int width )
{
  const int len = visible_width( s );

  if( len >= width ) return s;

  return s + std::string( width - len, ' ' );
}

std::string trunc( const std::string& s, int width )
{
  if( width <= 0 ) return {};

  if( visible_width( s ) <= width ) return s;

  if( width == 1 ) return "…";

  return s.substr( 0, width - 1 ) + "…";
}

// ============================================================
// TABLE HELPERS
// ============================================================

std::string border2( std::string_view left, std::string_view mid, std::string_view right, int w1, int w2 ) { return fmt::format( "{}{}{}{}{}\n", left, repeat( "─", w1 + 2 ), mid, repeat( "─", w2 + 2 ), right ); }

std::string border3( std::string_view left, std::string_view mid, std::string_view right, int w1, int w2, int w3 ) { return fmt::format( "{}{}{}{}{}{}{}\n", left, repeat( "─", w1 + 2 ), mid, repeat( "─", w2 + 2 ), mid, repeat( "─", w3 + 2 ), right ); }

// ============================================================
// NODE ID
// ============================================================

std::string node_id( simdjson::dom::element item )
{
  std::string component = "?";
  std::string type      = "?";
  std::string name      = "?";
  auto        id        = item["yaodaq_id"];
  if( !id.error() )
  {
    if( !id["component"].error() ) component = std::string( id["component"].get_string().value_unsafe() );
    if( !id["type"].error() ) type = std::string( id["type"].get_string().value_unsafe() );
    if( !id["name"].error() ) name = std::string( id["name"].get_string().value_unsafe() );
  }
  return component + "\\" + type + "\\" + name;
}

// ============================================================
// FORMATTER -> SINGLE TABLE CELL
// ============================================================

std::string table_cell( const simdjson::dom::element& e )
{
  std::string s = Formatter::format( e );
  std::replace( s.begin(), s.end(), '\n', ' ' );

  std::string out;
  out.reserve( s.size() );

  bool previous_space = false;

  for( char c: s )
  {
    if( std::isspace( static_cast<unsigned char>( c ) ) )
    {
      if( !previous_space )
      {
        out.push_back( ' ' );
        previous_space = true;
      }
    }
    else
    {
      out.push_back( c );
      previous_space = false;
    }
  }

  return out;
}

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

  const int width = std::max( 80, static_cast<int>( Term::screen_size().columns() ) );

  const int NODE_W   = std::max( 30, width / 3 );
  const int RESULT_W = std::max( 20, width - NODE_W - 9 );

  const int CODE_W = 10;

  const int MESSAGE_W = std::max( 20, width - NODE_W - CODE_W - 12 );

  auto blue = []( const std::string& s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::cornflower_blue ) ) ); };

  std::string ok;
  std::string err;

  // ==========================================================
  // SUCCESS TABLE
  // ==========================================================

  ok += fmt::format( "{}\n", fmt::styled( "SUCCESS RESULTS", fmt::fg( fmt::color::green ) | fmt::emphasis::bold ) );

  ok += border2( "┌", "┬", "┐", NODE_W, RESULT_W );

  ok += fmt::format( "│ {:<{}} │ {:<{}} │\n", "NODE", NODE_W, "RESULT", RESULT_W );

  ok += border2( "├", "┼", "┤", NODE_W, RESULT_W );

  // ==========================================================
  // ERROR TABLE
  // ==========================================================

  err += fmt::format( "{}\n", fmt::styled( "ERROR RESULTS", fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold ) );

  err += border3( "┌", "┬", "┐", NODE_W, CODE_W, MESSAGE_W );

  err += fmt::format( "│ {:<{}} │ {:<{}} │ {:<{}} │\n", "NODE", NODE_W, "CODE", CODE_W, "MESSAGE", MESSAGE_W );

  err += border3( "├", "┼", "┤", NODE_W, CODE_W, MESSAGE_W );

  // ==========================================================
  // DATA
  // ==========================================================

  for( auto item: simdjson::dom::array( root ) )
  {
    if( item["result"].error() ) continue;
    auto              rpc  = item["result"];
    const std::string node = trunc( node_id( item ), NODE_W );

    // --------------------------------------------------------
    // ERROR
    // --------------------------------------------------------

    if( !rpc["error"].error() )
    {
      auto        error   = rpc["error"];
      std::string code    = "?";
      std::string message = "?";

      if( !error["code"].error() ) code = table_cell( error["code"] );
      if( !error["message"].error() ) message = table_cell( error["message"] );

      err += fmt::format( "│ {} │ {} │ {} │\n", blue( pad( node, NODE_W ) ), pad( trunc( code, CODE_W ), CODE_W ), pad( trunc( message, MESSAGE_W ), MESSAGE_W ) );

      continue;
    }

    // --------------------------------------------------------
    // SUCCESS
    // --------------------------------------------------------

    std::string value = "?";

    if( !rpc["result"].error() ) value = table_cell( rpc["result"] );

    ok += fmt::format( "│ {} │ {} │\n", blue( pad( node, NODE_W ) ), pad( trunc( value, RESULT_W ), RESULT_W ) );
  }

  ok += border2( "└", "┴", "┘", NODE_W, RESULT_W );
  err += border3( "└", "┴", "┘", NODE_W, CODE_W, MESSAGE_W );
  return ok + "\n\n" + err;
}

}  // namespace yaodaq
