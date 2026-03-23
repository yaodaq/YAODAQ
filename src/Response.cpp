#include "yaodaq/Response.hpp"

#include <cpp-terminal/screen.hpp>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

// Main function to produce formatted results/errors
std::string yaodaq::Response::pretty_format()
{
  if( !m_json.is_array() ) return "No result array found\n";

  std::map<std::string, std::string> m_results;
  std::map<std::string, std::string> m_errors;

  // Collect results and errors
  for( const auto& item: m_json )
  {
    if( !item.is_object() ) continue;

    std::string id = item.contains( "yaodaq_id" ) ? item["yaodaq_id"].get<std::string>() : "<unknown>";

    if( item.contains( "error" ) )
    {
      std::string err_msg  = item["error"]["message"].get<std::string>();
      int         err_code = item["error"]["code"].get<int>();
      m_errors[id]         = fmt::format( "* {} {} ({})", id, err_msg, err_code );
    }
    else if( item.contains( "result" ) )
    {
      std::string res_str;
      if( item["result"].is_object() || item["result"].is_array() )
      {
        res_str = color_json( item["result"] );  // your existing color_json function
      }
      else
      {
        res_str = item["result"].dump();
      }
      m_results[id] = fmt::format( "[OK] {}\n{}", id, res_str );
    }
  }

  std::string output;
  std::size_t term_width = static_cast<std::size_t>( Term::screen_size().columns() );

  // Lambda to append rows with proper colors and alternating backgrounds
  auto append_rows = [&]( const std::map<std::string, std::string>& rows, fmt::color fg_color )
  {
    bool even = true;
    for( const auto& [id, text]: rows )
    {
      std::string line = text;
      // Remove newlines for single-line display
      line.erase( std::remove( line.begin(), line.end(), '\n' ), line.end() );

      // Prepare background style
      fmt::text_style bg_style = even ? fmt::text_style( fmt::bg( fmt::color::ghost_white ) ) : fmt::text_style( fmt::bg( fmt::color::light_slate_gray ) );

      // Now apply foreground + background in the same style to each piece
      // Example: for errors, split line into ID, message, code
      // We’ll just style the entire line with fg_color + bg
      std::string styled_line = fmt::format( "{}", fmt::styled( line, fmt::text_style( fmt::fg( fg_color ) | bg_style ) ) );

      output += styled_line + "\n";
      even = !even;
    }
  };

  // Append RESULTS
  if( !m_results.empty() )
  {
    output += fmt::format( "{:-^{}}", fmt::styled( " RESULTS ", fmt::fg( fmt::color::green ) ), term_width ) + "\n";
    append_rows( m_results, fmt::color::black );
  }

  // Append ERRORS
  if( !m_errors.empty() )
  {
    output += fmt::format( "{:-^{}}", fmt::styled( " ERRORS ", fmt::fg( fmt::color::red ) ), term_width ) + "\n";
    append_rows( m_errors, fmt::color::red );
  }

  return output;
}
