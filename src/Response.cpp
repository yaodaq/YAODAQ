#include "yaodaq/Response.hpp"

#include <algorithm>
#include <cpp-terminal/screen.hpp>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

// Assuming you have a Term class for terminal width
// and a color_json function for JSON syntax highlighting

#include <fmt/color.h>
#include <fmt/core.h>

using json = nlohmann::json;

// --- Color helpers ---
inline std::string key_color( const std::string& s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::cornflower_blue ) | fmt::emphasis::bold ) ); }

inline std::string string_color( const std::string& s ) { return fmt::format( "{}", fmt::styled( "\"" + s + "\"", fmt::fg( fmt::color::light_green ) ) ); }

inline std::string number_int_color( long long v ) { return fmt::format( "{}", fmt::styled( v, fmt::fg( fmt::color::plum ) ) ); }

inline std::string number_float_color( double v ) { return fmt::format( "{}", fmt::styled( v, fmt::fg( fmt::color::medium_purple ) ) ); }

inline std::string bool_color( bool v ) { return fmt::format( "{}", fmt::styled( v ? "true" : "false", fmt::fg( fmt::color::orange ) | fmt::emphasis::bold ) ); }

inline std::string null_color() { return fmt::format( "{}", fmt::styled( "null", fmt::fg( fmt::color::gray ) ) ); }

inline std::string punct( const std::string& s ) { return fmt::format( "{}", fmt::styled( s, fmt::fg( fmt::color::gray ) ) ); }

// --- Main printer ---
std::string pretty_printer( const json& j, int indent = 0 )
{
  std::string out;
  std::string pad( indent * 2, ' ' );

  if( j.is_object() )
  {
    out += punct( "{" ) + "\n";

    auto it = j.begin();
    while( it != j.end() )
    {
      out += pad + "  ";
      out += key_color( "\"" + it.key() + "\"" );
      out += punct( ": " );

      out += pretty_printer( it.value(), indent + 1 );

      ++it;
      if( it != j.end() ) out += punct( "," );
      out += "\n";
    }

    out += pad + punct( "}" );
  }
  else if( j.is_array() )
  {
    if( j.empty() ) { out += punct( "[]" ); }
    else
    {
      out += punct( "[" ) + "\n";

      for( size_t i = 0; i < j.size(); ++i )
      {
        out += pad + "  ";
        out += pretty_printer( j[i], indent + 1 );

        if( i + 1 != j.size() ) out += punct( "," );
        out += "\n";
      }

      out += pad + punct( "]" );
    }
  }
  else if( j.is_string() ) { out += string_color( j.get<std::string>() ); }
  else if( j.is_number_integer() ) { out += number_int_color( j.get<long long>() ); }
  else if( j.is_number_float() ) { out += number_float_color( j.get<double>() ); }
  else if( j.is_boolean() ) { out += bool_color( j.get<bool>() ); }
  else if( j.is_null() ) { out += null_color(); }

  return out;
}

std::string yaodaq::Response::pretty_format()
{
  if( !m_json.is_array() ) return fmt::format( fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold, "No result array found\n" );
  return pretty_printer( m_json );
  /* std::map<std::string, std::string> m_results;
  std::map<std::string, std::string> m_errors;
  std::size_t                        term_width = static_cast<std::size_t>( Term::screen_size().columns() );

  // Collect results and errors
  for( const auto& item: m_json )
  {
    if( !item.is_object() ) continue;

    std::string id = item.contains( "yaodaq_id" ) ? item["yaodaq_id"]["component"].get<std::string>() : "<unknown>";

    if( item.contains( "error" ) )
    {
      std::string err_msg  = item["error"]["message"].get<std::string>();
      int         err_code = item["error"].contains( "code" ) ? item["error"]["code"].get<int>() : -1;
      m_errors[id]         = fmt::format( "{} | {} ({})", id, err_msg, err_code );
    }
    else if( item.contains( "result" ) )
    {
      std::string res_str;
      if( item["result"].is_object() || item["result"].is_array() ) { res_str = color_json( item["result"] ); }
      m_results[id] = fmt::format( "{}:\n{}", id, res_str );
    }
  }

  std::string output;

  // Lambda to append rows with proper colors and alternating backgrounds
  auto append_rows = [&]( const std::map<std::string, std::string>& rows, fmt::color fg_color, const std::string& title, fmt::color title_color )
  {
    if( rows.empty() ) return;

    // Add title with border
    output += fmt::format( "{}\n", fmt::format( fmt::fg( title_color ) | fmt::emphasis::bold, "{:─^{}}", fmt::format( " {} ", title ), term_width ) );

    bool even          = true;
    bool is_last_entry = false;
    for( const auto& [id, text]: rows )
    {
      is_last_entry = ( &text == &rows.rbegin()->second );

      // Split text into lines
      std::vector<std::string> lines;
      std::string              line;
      std::istringstream       stream( text );
      while( std::getline( stream, line, '\n' ) ) { lines.push_back( line ); }

      // For each line in the text
      for( size_t i = 0; i < lines.size(); ++i )
      {
        const std::string& current_line = lines[i];

        // Prepare background style
        fmt::text_style bg_style = even ? fmt::bg( fmt::color::ghost_white ) : fmt::bg( fmt::color::light_slate_gray );

        // Style the entire line with fg_color + bg
        std::string styled_line = fmt::format( "{}", fmt::styled( fmt::format( "│ {} ", current_line ), fmt::text_style( fmt::fg( fg_color ) | bg_style ) ) );

        // Add left border and padding
        output += fmt::format( "{}", styled_line );

        // Add newline unless it's the last line of the last entry
        if( !( i == lines.size() - 1 && is_last_entry ) ) { output += "\n"; }
      }
      even = !even;
    }
  };

  // Append RESULTS
  append_rows( m_results, fmt::color::black, " RESULTS ", fmt::color::green );

  // Append ERRORS
  append_rows( m_errors, fmt::color::white, " ERRORS ", fmt::color::red );

  // Add bottom border if there were results or errors
  if( !m_results.empty() || !m_errors.empty() ) { output += fmt::format( "{}\n", fmt::format( fmt::fg( fmt::color::light_slate_gray ), "{:─^{}}", "", term_width ) ); }

  return output;*/
}
