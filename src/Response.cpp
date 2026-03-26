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
std::string yaodaq::Response::pretty_format()
{
  if( !m_json.is_array() ) return fmt::format( fmt::fg( fmt::color::crimson ) | fmt::emphasis::bold, "No result array found\n" );

  std::map<std::string, std::string> m_results;
  std::map<std::string, std::string> m_errors;
  std::size_t                        term_width = static_cast<std::size_t>( Term::screen_size().columns() );

  // Collect results and errors
  for( const auto& item: m_json )
  {
    if( !item.is_object() ) continue;

    std::string id = item.contains( "yaodaq_id" ) ? item["yaodaq_id"].get<std::string>() : "<unknown>";

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

  return output;
}
