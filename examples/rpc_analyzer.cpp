#include "RPCDataAnalyzer.hpp"

#include <filesystem>
#include <iostream>
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
//*��need to add something when adding new functions

namespace fs = std::filesystem;

//  5. Main Function
int main( int argc, char** argv )
{
  RPCDataAnalyzer analyzer;
  if( argc > 1 )
  {
    // File mode: first argument is the ROOT file path
    std::string filename = argv[1];

    // Extract base name (without path and extension)
    fs::path    p( filename );
    std::string stem   = p.stem().string();  // e.g., "data" from "data.root"
    std::string outDir = stem + "_analysis";

    // Create output directory (if it doesn't exist)
    try
    {
      if( fs::create_directories( outDir ) ) { std::cout << "Created output directory: " << outDir << std::endl; }
      else
      {
        std::cout << "Using existing output directory: " << outDir << std::endl;
      }
    }
    catch( const fs::filesystem_error& e )
    {
      std::cerr << "Warning: Failed to create directory '" << outDir << "'. Output will be written to current directory.\n"
                << "Error: " << e.what() << std::endl;
      outDir.clear();  // fallback to current dir
    }
    analyzer.setOutputDir( outDir );
    analyzer.runFromFile( filename );
  }
  else
  {
    std::cout << "Give a filename !!!" << std::endl;
    return 1;
  }
  analyzer.finalize();
  return 0;
}
