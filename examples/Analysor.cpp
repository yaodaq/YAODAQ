#include "Data.hpp"
#include "RPCDataAnalyzer.hpp"
#include "RawData.hpp"
#include "TApplication.h"
#include "TSystem.h"
#include "fmt/chrono.h"
#include "fmt/std.h"

#include <CLI/CLI.hpp>
#include <TBufferJSON.h>
#include <chrono>
#include <cpp-terminal/color.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <filesystem>
#include <memory>
#include <yaodaq/Module.hpp>
#include <THttpServer.h>

class Analyser : public yaodaq::Module
{
public:
  Analyser( yaodaq::Config cfg, const std::string_view name ) : yaodaq::Module( cfg, "MyLovelyAnalyser", "Analyser" )
  {
    
    m_analyse.setEnableDelayCut(true);
    m_server= std::make_unique<THttpServer>("http:8080");
    Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );  //ROOT is doing bad stufs

  }

  ~Analyser() override
  {
    //if(m_thread.joinable())m_thread.join();
  }

  bool on_initialize() override
  {
    
    Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
    return true;
  }

  void onRawData( const std::unique_ptr<yaodaq::RawData> raw ) override
  {
    if( raw->topic() == "MPI::DCT::Singlets::Events" )
    {
      std::string raw2( reinterpret_cast<const char*>( raw->payload().data() ), raw->payload().size() );

      auto obj = TBufferJSON::FromJSON<DCT::Event>( raw2 );

      m_analyse.ProcessEvent( *obj );
      m_analyse.finalize();
      auto effi = m_analyse.getEfficiencies();
      info( "Event {}", obj->event_number );
      for( std::size_t i = 0; i != effi.size(); ++i ) { info( "Efficiency layer: {}, side: {}, {:.3f} ± {:.3f}", effi[i].getSource().getLayer(), effi[i].getSource().getSide(), effi[i].getEfficiency().efficiency(), effi[i].getEfficiency().error() ); }
      info( "\n" );
      gSystem->ProcessEvents();
    }
  }

  bool on_configure() override
  { 
    //m_analyse.ensureHistograms();
    //m_analyse.Register(m_server.get());
    Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );  //ROOT is doing bad stufs
    return true;
  }

  bool on_stop() override
  {
    clear();
    Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );  //ROOT is doing bad stufs
    return true;
  }
  void clear()
  {
    warn( "Clearing histograms" );
    reset_event();
    m_analyse.reset();
    Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );  //ROOT is doing bad stufs
  }

private:
  RPCDataAnalyzer m_analyse;
  //std::thread m_thread;
  std::unique_ptr<THttpServer> m_server{nullptr};
};

int main( int argc, char* argv[] )
try
{
  Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
  CLI::App app{ "YAODAQ client" };
  argv = app.ensure_utf8( argv );
  //TApplication rootApp( "ROOT", &argc, argv,nullptr,-1 );
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
  Analyser    module( cfg, "DCT" /*, canvas, layer */ );
  //client.setTLS("/home/work/YAODAQ-1/localhost.crt","/home/work/YAODAQ-1/localhost.key","NONE");
  std::size_t nbrCTLC{ 3 };
  Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press " << std::to_string( nbrCTLC ) << " times CTRL+C to stop" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
  module.link();
  while( true )
  {
    //gSystem->ProcessEvents();
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
        else if( key == Term::Key::c ) { module.clear(); }
        else
        {
          nbrCTLC = 3;
          Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press Ctrl+Q " << std::to_string( nbrCTLC ) << " times to quit" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
        }
        break;
      }
      default:
      {
        break;
      }
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
