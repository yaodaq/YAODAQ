#include "csv.hpp"

#include <CLI/CLI.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <filesystem>
#include <yaodaq/Exception.hpp>
#include <yaodaq/Module.hpp>

class TDC : public yaodaq::Module
{
public:
  TDC( yaodaq::ClientConfig cfg, const std::string_view name ) : yaodaq::Module( cfg, "TDC", "MPI" ) {}
  virtual bool run()
  {
    std::size_t retries{ 0 };
    std::string filePath = makeFileName( expectedIndex );
    while( !fileExists( filePath ) ) std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    auto lastSize = std::filesystem::file_size( filePath );
    while( true )
    {
      std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
      auto newSize = std::filesystem::file_size( filePath );
      if( newSize == lastSize ) break;
      lastSize = newSize;
    }
    do
    {
      logger()->info( "reading and parsing {}", filePath );
      try
      {
        csv::CSVReader reader( filePath.c_str() );
        nlohmann::json json;
        json["yaodaq"]         = true;
        json["type"]           = "rawdata";
        nlohmann::json bcids   = nlohmann::json::array();
        nlohmann::json payload = nlohmann::json::array();
        for( auto& row: reader )
        {
          //payload.push_back(std::stol(row["elink_out_tmp[27:0]"].get<std::string>(),nullptr,16));
          //bcids.push_back(std::stol(row["bcid320[11:0]"].get<std::string>(),nullptr,16));
        }
        json["rawdata"]["bcouts"] = bcids;
        json["rawdata"]["words"]  = payload;
        send( json.dump() );
        std::filesystem::remove( filePath );
        logger()->warn( "removed: {}", filePath );
      }
      catch( const std::exception& e )
      {
        logger()->warn( "read failed ({}), retry {}", e.what(), ++retries );
        std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
        continue;
      }
      catch( ... )
      {
        logger()->warn( "read failed retry {}", ++retries );
        std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
      }
      ++expectedIndex;
      return true;
    } while( true );
  }

  bool on_start() override
  {
    createTempDirectory();
    //std::cout<<"HERE "<<m_temp.c_str()<<std::endl;
    std::string   script = fmt::format( u8R"(set nEvts 1000000
    set inputDir BI_DCT_FW
    set outputDir DCT_raw_files
    set inputPath ./$inputDir
    # channel mask
    # . layer2. layer1. layer0
    set eta1_1 000000000000000000000000
    set eta1_2 000000000000000000000000
    set eta1_3 000000000000000000000000
    set eta1_4 000000000000000000000000
    set eta1_5 000000000000000000000000
    set eta1_6 000000000000000000000000
    #          . layer2. layer1. layer0
    set eta2_1 000000000000000000000000
    set eta2_2 000000000000000000000000
    set eta2_3 000000000000000000000000
    set eta2_4 000000000000000000000000
    set eta2_5 000000000000000000000000
    set eta2_6 000000000000000000000000
    set mask_eta1 $eta1_6$eta1_5$eta1_4$eta1_3$eta1_2$eta1_1
    set mask_eta2 $eta2_6$eta2_5$eta2_4$eta2_3$eta2_2$eta2_1
    open_hw
    connect_hw_server
    open_hw_target
    current_hw_device [get_hw_devices xc7a200t_0]
    refresh_hw_device -update_hw_probes false [lindex [get_hw_devices xc7a200t_0] 0]
    set_property PROBES.FILE $inputPath/top.ltx [get_hw_devices xc7a200t_0]
    set_property FULL_PROBES.FILE $inputPath/top.ltx [get_hw_devices xc7a200t_0]
    set_property PROGRAM.FILE $inputPath/top.bit [get_hw_devices xc7a200t_0]
    program_hw_devices [get_hw_devices xc7a200t_0]
    refresh_hw_device [lindex [get_hw_devices xc7a200t_0] 0]
    display_hw_ila_data [ get_hw_ila_data hw_ila_data_2 -of_objects [get_hw_ilas -of_objects [get_hw_devices xc7a200t_0] -filter {{CELL_NAME=~"ila_elinks_inst"}}]]
    set_property CONTROL.DATA_DEPTH 128 [get_hw_ilas -of_objects [get_hw_devices xc7a200t_0] -filter {{CELL_NAME=~"ila_elinks_inst"}}]
    set_property CONTROL.TRIGGER_POSITION 30 [get_hw_ilas -of_objects [get_hw_devices xc7a200t_0] -filter {{CELL_NAME=~"ila_elinks_inst"}}]
    set_property TRIGGER_COMPARE_VALUE eq1'bR [get_hw_probes SMA_in_buf  -of_objects [get_hw_ilas -of_objects [get_hw_devices xc7a200t_0] -filter {{CELL_NAME=~"ila_elinks_inst"}}]]
    set_property OUTPUT_VALUE_RADIX BINARY [get_hw_probes channel_mask_left -of_objects [get_hw_vios -of_objects [get_hw_devices xc7a200t_0] -filter {{CELL_NAME=~"ctrl_word_vio"}}]]
    set_property OUTPUT_VALUE $mask_eta1 [get_hw_probes channel_mask_left -of_objects [get_hw_vios -of_objects [get_hw_devices xc7a200t_0]	-filter	{{CELL_NAME=~"ctrl_word_vio"}}]]
    commit_hw_vio [get_hw_probes channel_mask_left -of_objects [get_hw_vios -of_objects [get_hw_devices xc7a200t_0]	-filter	{{CELL_NAME=~"ctrl_word_vio"}}]]
    set_property OUTPUT_VALUE_RADIX BINARY [get_hw_probes channel_mask_right -of_objects [get_hw_vios -of_objects [get_hw_devices xc7a200t_0] -filter {{CELL_NAME=~"ctrl_word_vio"}}]]
    set_property OUTPUT_VALUE $mask_eta2 [get_hw_probes channel_mask_right -of_objects [get_hw_vios -of_objects [get_hw_devices xc7a200t_0]  -filter {{CELL_NAME=~"ctrl_word_vio"}}]]
    commit_hw_vio [get_hw_probes channel_mask_right -of_objects [get_hw_vios -of_objects [get_hw_devices xc7a200t_0] -filter {{CELL_NAME=~"ctrl_word_vio"}}]]
    for {{set i 0}} {{$i < $nEvts}} {{incr i}} {{
      run_hw_ila [get_hw_ilas -of_objects [get_hw_devices xc7a200t_0] -filter {{CELL_NAME=~"ila_elinks_inst"}}]
      wait_on_hw_ila [get_hw_ilas -of_objects [get_hw_devices xc7a200t_0] -filter {{CELL_NAME=~"ila_elinks_inst"}}]
      display_hw_ila_data [upload_hw_ila_data [get_hw_ilas -of_objects [get_hw_devices xc7a200t_0] -filter {{CELL_NAME=~"ila_elinks_inst"}}]]
      write_hw_ila_data -legacy_csv_file -force -quiet {}/event_$i hw_ila_data_2
      if {{($i % 10)==0}} {{
          puts "Events: $i / $nEvts"
    }}
    }}
    close_hw_manager
    exit)",
                                        m_temp.c_str() );
    std::ofstream script_file( std::string( m_temp.c_str() ) + std::string( "script.tcl" ) );
    script_file << script;
    script_file.close();
    //std::cout<<script<<std::endl;
    std::string path{ "export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/tools/Xilinx/2025.1.1/Vivado/bin:/tools/Xilinx/2025.1.1/Vivado/bin" };
    bool        ret = std::system( fmt::format( "{}\nvivado -mode batch -source {}", path, std::string( m_temp.c_str() ) + std::string( "script.tcl" ) ).c_str() );
    // std::cout<<"HERE2"<<std::endl;
    return ret;
  }

private:
  std::string makeFileName( int index ) { return m_temp += "/event_" + std::to_string( index ) + ".csv"; }

  bool fileExists( const std::string& path ) { return std::filesystem::exists( path ); }
  bool createTempDirectory()
  {
    m_temp = std::filesystem::temp_directory_path() /= std::tmpnam( nullptr );
    std::filesystem::create_directories( m_temp );
    return true;
  }

  std::atomic<int>      expectedIndex{ 0 };
  std::mutex            mtx;
  std::filesystem::path m_temp;
};

int main( int argc, char* argv[] )
try
{
  Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
  CLI::App app{ "YAODAQ client" };
  argv = app.ensure_utf8( argv );
  std::string host{ "127.0.0.1" };
  app.add_option( "-i,--ip", host, "IP of the server" ) /*->check( CLI::ValidIPV4 )*/;
  int port{ 8888 };
  app.add_option( "-p,--port", port, "Port to listen" )->check( CLI::Range( 0, 65535 ) );
  std::string path;
  app.add_option( "--path", path, "path to read the CSV" );

  try
  {
    app.parse( argc, argv );
  }
  catch( const CLI::ParseError& e )
  {
    return app.exit( e );
  }
  yaodaq::ClientConfig cfg;
  cfg().setPort( port ).setHost( host );
  TDC module( cfg, "TDC" );
  //client.setTLS("/home/work/YAODAQ-1/localhost.crt","/home/work/YAODAQ-1/localhost.key","NONE");
  module.link();

  Term::cout << "Press 3 times CTRL+C to stop" << std::endl;
  std::size_t nbrCTLC{ 0 };
  while( true )
  {
    Term::Event event = Term::read_event();
    switch( event.type() )
    {
      case Term::Event::Type::Key:
      {
        Term::Key key( event );
        if( key == Term::Key::Ctrl_Q )
        {
          ++nbrCTLC;
          if( nbrCTLC == 3 ) return 0;
        }
        else
        {
          nbrCTLC = 0;
        }
        break;
      }
      default: break;
    }
  };
  return 0;
}
catch( const yaodaq::Exception& exception )
{
  //spdlog::error( "{}", exception.what() );
}
catch( const std::exception& exception )
{
  //spdlog::error( "{}", exception.what() );
}
catch( ... )
{
  //spdlog::error( "Exception thrown" );
}
