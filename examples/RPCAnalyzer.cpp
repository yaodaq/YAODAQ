#include "RPCDataAnalyzer.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string>

// =====================================================================
//  RPC Data Analyzer - Code Structure (Index)
// =====================================================================
//  1. Configuration & Constants*
//  2. Data Structures (Hit, EventData)
//  3. Analyzer Class
//     3.1 Constructor* & Destructor*
//     3.2 Configuration (setOutputDir)
//     3.3 Event Processing (processEvent)
//         3.3.x Functions*
//     3.4 Finalization (finalize)
//         3.4.x Functions*
//     3.5 File Processing (runFromFile)
//     3.6 Realtime Mode (runRealtime)
//  4. Private Member Variables*
//  5. Main Function
// =====================================================================
// need to add something when adding new functions
namespace fs = std::filesystem;

int main( int argc, char** argv )
{
  CLI::App app{ "RPC Analyzer" };
  argv = app.ensure_utf8( argv );
  std::string file_name{ "" };
  app.add_option( "-f,--file_name", file_name, "File to analyse" )->required()->check( CLI::ExistingFile );
  try
  {
    app.parse( argc, argv );
  }
  catch( const CLI::ParseError& e )
  {
    return app.exit( e );
  }
  RPCDataAnalyzer analyzer;
  // Extract base name (without path and extension)
  fs::path        p( file_name );
  std::string     stem   = p.stem().string();  // e.g., "data" from "data.root"
  std::string     outDir = stem + "_analysis";

  // Create output directory (if it doesn't exist)
  try
  {
    if( fs::create_directories( outDir ) ) { std::cout << "Created output directory: " << outDir << std::endl; }
    else
      spdlog::info( "Using existing output directory: {}", outDir );
  }
  catch( const fs::filesystem_error& e )
  {
    spdlog::warn( "Error: {}\nFailed to create directory {}.\nOutput will be written to current directory.\n", e.what(), outDir );
    outDir.clear();  // fallback to current dir
  }
  analyzer.setOutputDir( outDir );
  analyzer.runFromFile( file_name );
  analyzer.finalize();
  return 0;
}
