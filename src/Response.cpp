#include "yaodaq/Response.hpp"

#include "yaodaq/Formatter.hpp"

#include <algorithm>
#include <cpp-terminal/screen.hpp>
#include <fmt/color.h>
#include <simdjson.h>
#include <string>
#include <string_view>
#include <vector>

namespace yaodaq
{

namespace
{

// ============================================================
// STRING HELPERS
// ============================================================

std::string repeat( const std::string_view s, const std::size_t n )
{
  std::string out;
  out.reserve( s.size() * n );

  for( std::size_t i = 0; i < n; ++i ) out += s;

  return out;
}

// Count visible characters ignoring ANSI escape sequences
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

std::string pad( const std::string_view s, const std::size_t width )
{
  const auto len = visible_width( s );

  if( len >= width ) return std::string( s );

  return std::string( s ) + std::string( width - len, ' ' );
}

// ANSI-aware line wrapping
std::vector<std::string> wrap( const std::string_view s, const std::size_t width )
{
  std::vector<std::string> lines;

  if( width == 0 )
  {
    lines.emplace_back();
    return lines;
  }

  std::string current;
  std::size_t current_width = 0;
  bool        escape        = false;

  for( const char c: s )
  {
    current += c;

    if( escape )
    {
      if( c == 'm' ) escape = false;

      continue;
    }

    if( c == '\033' )
    {
      escape = true;
      continue;
    }

    if( c == '\n' )
    {
      current.pop_back();

      lines.push_back( current );

      current.clear();
      current_width = 0;

      continue;
    }

    ++current_width;

    if( current_width >= width )
    {
      lines.push_back( current );

      current.clear();
      current_width = 0;
    }
  }

  if( !current.empty() ) lines.push_back( current );

  if( lines.empty() ) lines.emplace_back();

  return lines;
}

// ============================================================
// TABLE BORDERS
// ============================================================

std::string border2( const std::string_view left, const std::string_view mid, const std::string_view right, const std::size_t w1, const std::size_t w2 ) { return fmt::format( "{}{}{}{}{}\n", left, repeat( "─", w1 + 2 ), mid, repeat( "─", w2 + 2 ), right ); }

std::string border3( const std::string_view left, const std::string_view mid, const std::string_view right, const std::size_t w1, const std::size_t w2, const std::size_t w3 )
{ return fmt::format( "{}{}{}{}{}{}{}\n", left, repeat( "─", w1 + 2 ), mid, repeat( "─", w2 + 2 ), mid, repeat( "─", w3 + 2 ), right ); }

// ============================================================
// COLORS
// ============================================================

std::string blue( const std::string_view s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::cornflower_blue ) ) ); }

std::string green_title( const std::string_view s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::green ) | fmt::emphasis::bold ) ); }

std::string red_title( const std::string_view s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold ) ); }

// These are local copies to avoid depending on Formatter internals

std::string key_color( const std::string_view s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::cornflower_blue ) | fmt::emphasis::bold ) ); }

std::string punct( const std::string_view s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::gray ) ) ); }

// ============================================================
// JSON HELPERS
// ============================================================

// Forward declaration
std::string table_cell( const simdjson::dom::element& e, std::size_t indent = 0 );

// Try to interpret a string value as JSON.
// Prevent recursion if the parsed JSON is only another string.
std::string format_embedded_json( const std::string_view s, const std::size_t indent )
{
  static thread_local simdjson::dom::parser parser;

  simdjson::dom::element root;

  if( parser.parse( s ).get( root ) != simdjson::SUCCESS ) return {};

  // Avoid endless recursion:
  // "\"hello\"" -> "hello" -> "\"hello\"" ...
  if( root.type() == simdjson::dom::element_type::STRING ) return {};

  return table_cell( root, indent );
}

// Convert simdjson element to colored formatted text
std::string table_cell( const simdjson::dom::element& e, const std::size_t indent )
{
  std::string out;

  const std::string indentation( indent * 2, ' ' );

  switch( e.type() )
  {
      // --------------------------------------------------------
      // OBJECT
      // --------------------------------------------------------

    case simdjson::dom::element_type::OBJECT:
    {
      const simdjson::dom::object obj = e;

      out += "{";

      bool first = true;

      for( const auto [key, value]: obj )
      {
        if( first )
        {
          out += "\n";
          first = false;
        }
        else
        {
          out += ",\n";
        }

        out += std::string( ( indent + 1 ) * 2, ' ' );

        out += key_color( fmt::format( "\"{}\"", std::string_view( key ) ) );

        out += punct( ": " );

        out += table_cell( value, indent + 1 );
      }

      if( !first )
      {
        out += "\n";
        out += indentation;
      }

      out += "}";

      break;
    }

      // --------------------------------------------------------
      // ARRAY
      // --------------------------------------------------------

    case simdjson::dom::element_type::ARRAY:
    {
      const simdjson::dom::array arr = e;

      out += "[";

      bool first = true;

      for( const auto value: arr )
      {
        if( first )
        {
          out += "\n";
          first = false;
        }
        else
        {
          out += ",\n";
        }

        out += std::string( ( indent + 1 ) * 2, ' ' );

        out += table_cell( value, indent + 1 );
      }

      if( !first )
      {
        out += "\n";
        out += indentation;
      }

      out += "]";

      break;
    }

      // --------------------------------------------------------
      // STRING
      // --------------------------------------------------------

    case simdjson::dom::element_type::STRING:
    {
      const auto value = e.get_string().value_unsafe();

      if( auto json = format_embedded_json( value, indent ); !json.empty() ) { out += json; }
      else
      {
        out += string_color( value );
      }

      break;
    }

      // --------------------------------------------------------
      // NUMBERS
      // --------------------------------------------------------

    case simdjson::dom::element_type::INT64:
    {
      out += number_int_color( e.get_int64().value_unsafe() );

      break;
    }

    case simdjson::dom::element_type::UINT64:
    {
      out += fmt::format( "{}", fmt::styled( e.get_uint64().value_unsafe(), fmt::fg( fmt::color::plum ) ) );

      break;
    }

    case simdjson::dom::element_type::DOUBLE:
    {
      out += number_float_color( e.get_double().value_unsafe() );

      break;
    }

      // --------------------------------------------------------
      // BOOLEAN / NULL
      // --------------------------------------------------------

    case simdjson::dom::element_type::BOOL:
    {
      out += bool_color( e.get_bool().value_unsafe() );

      break;
    }

    case simdjson::dom::element_type::NULL_VALUE:
    {
      out += null_color();

      break;
    }

    default:
    {
      out += string_color( "[unsupported]" );

      break;
    }
  }

  return out;
}

// ============================================================
// NODE IDENTIFIER
// ============================================================

std::string node_id( const simdjson::dom::element& item )
{
  std::string component = "?";
  std::string type      = "?";
  std::string name      = "?";

  const auto id = item["yaodaq_id"];

  if( !id.error() )
  {
    if( !id["component"].error() ) { component = std::string( id["component"].get_string().value_unsafe() ); }

    if( !id["type"].error() ) { type = std::string( id["type"].get_string().value_unsafe() ); }

    if( !id["name"].error() ) { name = std::string( id["name"].get_string().value_unsafe() ); }
  }

  return fmt::format( R"({}\{}\{})", component, type, name );
}

// ============================================================
// TABLE ROW RENDERING
// ============================================================

void append_row( std::string& out, const std::vector<std::string>& cells, const std::vector<std::size_t>& widths, const bool color_first_column )
{
  std::vector<std::vector<std::string>> wrapped;

  std::size_t height = 1;

  for( std::size_t i = 0; i < cells.size(); ++i )
  {
    auto lines = wrap( cells[i], widths[i] );

    height = std::max( height, lines.size() );

    wrapped.push_back( std::move( lines ) );
  }

  for( std::size_t line = 0; line < height; ++line )
  {
    out += "│ ";

    for( std::size_t col = 0; col < cells.size(); ++col )
    {
      std::string value;

      if( line < wrapped[col].size() ) value = wrapped[col][line];

      // Hide first column after first line
      if( col == 0 && line > 0 ) value.clear();

      if( color_first_column && col == 0 && line == 0 ) { value = blue( value ); }

      out += pad( value, widths[col] );

      if( col + 1 != cells.size() ) out += " │ ";
    }

    out += " │\n";
  }
}

// ============================================================
// RESULT ROW
// ============================================================

void append_result_row( std::string& out, const std::string& node, const std::string& result, const std::size_t node_width, const std::size_t result_width ) { append_row( out, { pad( node, node_width ), result }, { node_width, result_width }, true ); }

// ============================================================
// ERROR ROW
// ============================================================

void append_error_row( std::string& out, const std::string& node, const std::string& code, const std::string& message, const std::size_t node_width, const std::size_t code_width, const std::size_t message_width )
{ append_row( out, { pad( node, node_width ), code, message }, { node_width, code_width, message_width }, true ); }

}  // anonymous namespace

// ============================================================
// TABULATE
// ============================================================

std::string Response::tabulate()
{
  static thread_local simdjson::dom::parser parser;

  simdjson::dom::element root;

  if( parser.parse( m_raw ).get( root ) != simdjson::SUCCESS ) { return fmt::format( fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold, "Invalid JSON-RPC response\n" ); }

  if( root.type() != simdjson::dom::element_type::ARRAY ) { return fmt::format( fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold, "Expected JSON-RPC batch array\n" ); }

  const std::size_t terminal_width = std::max<std::size_t>( 80, static_cast<std::size_t>( Term::screen_size().columns() ) );

  const std::size_t node_width = std::max<std::size_t>( 30, terminal_width / 3 );

  const std::size_t code_width = 10;

  const std::size_t result_width = std::max<std::size_t>( 20, terminal_width > node_width + 8 ? terminal_width - node_width - 9 : 20 );

  const std::size_t message_width = std::max<std::size_t>( 20, terminal_width > node_width + code_width + 12 ? terminal_width - node_width - code_width - 12 : 20 );

  std::string results;
  std::string errors;

  // ========================================================
  // RESULTS HEADER
  // ========================================================

  results += green_title( "Results" );
  results += "\n";

  results += border2( "┌", "┬", "┐", node_width, result_width );

  results += fmt::format( "│ {:<{}} │ {:<{}} │\n", "Client", node_width, "Result", result_width );

  results += border2( "├", "┼", "┤", node_width, result_width );

  // ========================================================
  // ERRORS HEADER
  // ========================================================

  errors += red_title( "Errors" );
  errors += "\n";

  errors += border3( "┌", "┬", "┐", node_width, code_width, message_width );

  errors += fmt::format( "│ {:<{}} │ {:<{}} │ {:<{}} │\n", "Node", node_width, "Code", code_width, "Message", message_width );

  errors += border3( "├", "┼", "┤", node_width, code_width, message_width );

  // ========================================================
  // ROWS
  // ========================================================
  bool has_error{ false };
  bool has_result{ false };
  for( auto item: simdjson::dom::array( root ) )
  {
    const auto rpc = item["result"];

    if( rpc.error() ) continue;

    const auto node = node_id( item );

    // ----------------------------------------------------
    // ERROR
    // ----------------------------------------------------

    const auto error = rpc["error"];
    if( !error.error() )
    {
      has_error           = true;
      std::string code    = "?";
      std::string message = "?";

      auto code_field = error["code"];

      if( !code_field.error() ) { code = table_cell( code_field.value() ); }

      auto message_field = error["message"];

      if( !message_field.error() ) { message = table_cell( message_field.value() ); }

      append_error_row( errors, node, code, message, node_width, code_width, message_width );

      continue;
    }

    // ----------------------------------------------------
    // SUCCESS
    // ----------------------------------------------------

    std::string result = "?";

    auto result_field = rpc["result"];

    if( !result_field.error() )
    {
      has_result = true;
      result     = table_cell( result_field.value() );
    }

    append_result_row( results, node, result, node_width, result_width );
  }

  // ========================================================
  // FOOTERS
  // ========================================================

  results += border2( "└", "┴", "┘", node_width, result_width );

  errors += border3( "└", "┴", "┘", node_width, code_width, message_width );
  if( has_result && has_error ) return results + "\n\n" + errors;
  else if( has_result && !has_error )
    return results;
  else if( !has_result && has_error )
    return errors;
  else
    return "";
}

}  // namespace yaodaq
