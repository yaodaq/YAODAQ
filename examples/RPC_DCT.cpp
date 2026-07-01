#include "csv.hpp"

#include <CLI/CLI.hpp>
#include <cpp-terminal/color.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <fcntl.h>
#include <filesystem>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <sys/wait.h>
#include <unistd.h>
#include <yaodaq/Exception.hpp>
#include <yaodaq/Module.hpp>

class DCT : public yaodaq::Module
{
public:
  DCT( yaodaq::Config cfg, const std::string_view name ) : yaodaq::Module( cfg, "DCT", "MPI" )
  {
    setRun( [this]( std::stop_token stop ) { return run( stop ); } );
  }

  virtual ~DCT()
  {
    info( "DCT shutdown" );

    stopVivado();

    if( m_vivado_thread.joinable() )
    {
      m_vivado_thread.request_stop();
      m_vivado_thread.join();
    }

    info( "DCT shutdown complete" );
  }
  virtual bool run( std::stop_token stop )
  {
    while( !stop.stop_requested() )
    {
      std::cout << "SSSSSS runingngngngngng" << std::endl;
      std::size_t retries{ 0 };
      std::string filePath = makeFileName( expectedIndex );
      while( !stop.stop_requested() && !fileExists( filePath ) ) std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
      auto lastSize = std::filesystem::file_size( filePath );
      while( !stop.stop_requested() )
      {
        std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
        auto newSize = std::filesystem::file_size( filePath );
        if( newSize == lastSize ) break;
        lastSize = newSize;
      }
      info( "reading and parsing {}", filePath );
      try
      {
        csv::CSVReader reader( filePath.c_str() );
        nlohmann::json json;
        nlohmann::json bcids   = nlohmann::json::array();
        nlohmann::json payload = nlohmann::json::array();
        for( auto& row: reader )
        {
          payload.push_back( row["elink_out_tmp[27:0]"].get<std::string>() );
          bcids.push_back( row["bcid320[11:0]"].get<std::string>() );
        }
        json["rawdata"]["bcouts"] = bcids;
        json["rawdata"]["words"]  = payload;
        std::cout << json.dump( 2 ) << std::endl;
        send( yaodaq::RawDataBuilder::from_text( json.dump(), "event" ) );
        //std::filesystem::remove( filePath );
        warn( "removed: {}", filePath );
      }
      catch( const std::exception& e )
      {
        warn( "read failed ({}), retry {}", e.what(), ++retries );
        std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
        continue;
      }
      catch( ... )
      {
        warn( "read failed retry {}", ++retries );
        std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
      }
      ++expectedIndex;
      return true;
    }
    return true;
  }

  bool on_start() override
  {
    std::cout << "FFFFFFFFFFFFFFFFFFFFFFFFff" << std::endl;

    createTempDirectory();
    std::string base = m_temp;
    //set inputPath {}
    //set outputPath {}
    if( !base.ends_with( '/' ) ) base += '/';
    info( "DCT Rawdata (csv) will be written in {}", m_temp );
    // ---- Build TCL script as a raw string template ----
    std::string script = fmt::format( R"(set nEvts {}
    set inputPath {}
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
                                      m_events, m_binary_path.c_str(), m_temp.c_str() );

    // ---- Write file ----
    std::ofstream script_file( base + "script.tcl" );
    script_file << script;
    script_file.close();

    info( "Vivado script has been written and will be launched!" );
    if( !m_vivado_path.ends_with( '/' ) ) m_vivado_path += '/';

    auto result = runProcessCapture( m_vivado_thread, fmt::format( "{}vivado", m_vivado_path ), { "-mode", "batch", "-source", ( m_temp / "script.tcl" ).string() } );

    Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
    if( result.exitCode != 0 )
    {
      error( "Vivado failed with exit code {}", result.exitCode );
      return false;
    }
    else
    {
      info( "Process terminated successfully" );
      return true;
    }
  }
  void setBinaryPath( const std::string& binary_path ) { m_binary_path = binary_path; }
  void setEventNumbers( const std::size_t event ) { m_events = event; }
  void setVivadoPath( const std::string& vivado ) { m_vivado_path = vivado; }

  virtual bool on_stop()
  {
    stopVivado();
    return true;
  }

private:
  struct ProcessResult
  {
    int         exitCode;
    std::string output;
  };

  ProcessResult runProcessCapture( std::jthread& m_vivado_thread, const std::string& exe, const std::vector<std::string>& args )
  {
    info( "Running : {} {}", exe, fmt::join( args, " " ) );
    int pipefd[2];
    if( pipe( pipefd ) < 0 )  // pipefd[0] = read, pipefd[1] = write
      return { -1, "pipe failed" };

    pid_t pid = fork();
    if( pid < 0 ) return { -1, "fork failed" };

    if( pid == 0 )
    {
      // CHILD
      // Create a new process group
      // Create a new session + process group (VERY IMPORTANT)
      if( setsid() < 0 ) _exit( 127 );
      ::dup2( pipefd[1], STDOUT_FILENO );
      ::dup2( pipefd[1], STDERR_FILENO );

      ::close( pipefd[0] );
      ::close( pipefd[1] );

      std::vector<char*> argv;
      argv.push_back( const_cast<char*>( exe.c_str() ) );

      for( auto& a: args ) argv.push_back( const_cast<char*>( a.c_str() ) );

      argv.push_back( nullptr );

      ::execvp( exe.c_str(), argv.data() );

      ::_exit( 127 );
    }
    m_vivado_pid = pid;
    // PARENT
    ::close( pipefd[1] );
    ::fcntl( pipefd[0], F_SETFL, O_NONBLOCK );
    if( m_vivado_thread.joinable() )
    {
      m_vivado_thread.request_stop();
      m_vivado_thread.join();
    }
    m_vivado_thread = std::jthread(
      [this, fd = pipefd[0]]( std::stop_token st ) mutable
      {
        std::string line;
        char        tmp[4096];
        ssize_t     n;

        while( !st.stop_requested() )
        {
          ssize_t n = read( fd, tmp, sizeof( tmp ) );

          if( n > 0 )
          {
            for( ssize_t i = 0; i < n; ++i )
            {
              char c = tmp[i];
              if( c == '\n' )
              {
                if( !line.empty() )
                {
                  info( "Vivado: {}", line );
                  line.clear();
                }
              }
              else
                line += c;
            }
          }
          else
          {
            std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
          }
        }

        if( !line.empty() ) info( "Vivado: {}", line );

        ::close( fd );
      } );

    return { 0, "" };
  }

  std::size_t m_events{ 1000 };
  std::string makeFileName( const int index ) { return ( m_temp / fmt::format( "event_{}.csv", index ) ).string(); }

  bool fileExists( const std::string& path ) { return std::filesystem::exists( path ); }
  bool createTempDirectory()
  {
    char  dirTemplate[] = "/tmp/DCTRawData_XXXXXX";
    char* result        = mkdtemp( dirTemplate );
    std::cout << result << std::endl;
    if( result )
    {
      m_temp = result;
      return true;
    }
    return false;
  }

  std::atomic<int>      expectedIndex{ 0 };
  std::mutex            mtx;
  std::filesystem::path m_temp;
  std::string           m_binary_path{ "./" };
  std::string           m_vivado_path{ "/tools/Xilinx/2025.1.1/Vivado/bin" };
  pid_t                 m_vivado_pid{ -1 };
  std::jthread          m_vivado_thread;
  void                  stopVivado()
  {
    if( m_vivado_pid <= 0 ) return;

    info( "Stopping Vivado process group" );

    kill( -m_vivado_pid, SIGTERM );

    for( int i = 0; i < 20; ++i )
    {
      if( waitpid( m_vivado_pid, nullptr, WNOHANG ) == m_vivado_pid )
      {
        m_vivado_pid = -1;
        return;
      }
      std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }

    warn( "Vivado not responding → SIGKILL" );

    kill( -m_vivado_pid, SIGKILL );
    waitpid( m_vivado_pid, nullptr, 0 );

    m_vivado_pid = -1;
  }
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
  std::string path_binary{ "/home/user/Desktop/Mattia_python/bi_dct_data_acquisition_update_M/BI_DCT_FW/" };
  app.add_option( "--path", path_binary, "binaries path to load to the DCT" );
  std::size_t event_number{ 1000 };
  app.add_option( "-e,--events", event_number, "Number of events to take" );
  std::string vivado_path{ "/opt/vivado/2025.2/Vivado/bin/" };
  app.add_option( "--vivado", vivado_path, "Vivado installation path" );
  try
  {
    app.parse( argc, argv );
  }
  catch( const CLI::ParseError& e )
  {
    return app.exit( e );
  }
  yaodaq::Config cfg;
  cfg.setPort( port ).setHost( host );
  DCT module( cfg, "DCT" );
  module.setBinaryPath( path_binary );
  module.setEventNumbers( event_number );
  module.setVivadoPath( vivado_path );
  //client.setTLS("/home/work/YAODAQ-1/localhost.crt","/home/work/YAODAQ-1/localhost.key","NONE");
  module.link();

  std::size_t nbrCTLC{ 3 };
  Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press " << std::to_string( nbrCTLC ) << " times CTRL+C to stop" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
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
          --nbrCTLC;
          if( nbrCTLC == 0 ) return 0;
          else
            Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press Ctrl+Q " << std::to_string( nbrCTLC ) << " times to quit" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
        }
        else
        {
          nbrCTLC = 3;
          Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press Ctrl+Q " << std::to_string( nbrCTLC ) << " times to quit" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
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
  Term::cerr << Term::color_fg( Term::Color::Name::Red ) << exception.what() << Term::color_fg( Term::Color::Name::Default ) << std::endl;
}
catch( const std::exception& exception )
{
  Term::cerr << Term::color_fg( Term::Color::Name::Red ) << exception.what() << Term::color_fg( Term::Color::Name::Default ) << std::endl;
}
catch( ... )
{
  Term::cerr << Term::color_fg( Term::Color::Name::Red ) << "error" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
}
