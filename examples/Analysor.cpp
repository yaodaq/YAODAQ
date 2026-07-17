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

class Analyser : public yaodaq::Module
{
public:
  Analyser( yaodaq::Config cfg, const std::string_view name ) : yaodaq::Module( cfg, "MyLovelyAnalyser", "Analyser" )
  {
    Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );  //ROOT is doing bad stufs
  }

  ~Analyser() override {}

  void onRawData( const std::unique_ptr<yaodaq::RawData> raw ) override
  {
    if( raw->topic() == "MPI::DCT::Singlets::Events" )
    {
      std::string raw2( reinterpret_cast<const char*>( raw->payload().data() ), raw->payload().size() );

      auto obj = TBufferJSON::FromJSON<DCT::Event>( raw2 );

      m_analyse.ProcessEvent( *obj, layer );
      if( m_can )
      {
        m_can->Modified();
        m_can->Update();
      }
      else
      {
        createCanvas();
        createPlots();
        if( m_can ) m_can->Draw();
        if( layer ) layer->Draw();
      }
    }
  }

  bool on_configure() override
  {
    bool good{ true };
    if( !createCanvas() ) return false;
    if( !createPlots() ) return false;
    if( m_can ) m_can->Draw();
    if( layer ) layer->Draw();
    return true;
  }

  bool on_stop() override
  {
    m_analyse.finalize();
    return true;
  }
  void clear()
  {
    warn( "Clearing histograms" );
    layer->Reset();
    m_can->Modified();
    m_can->Update();
  }

private:
  bool createCanvas()
  {
    if( !m_can ) m_can = new TCanvas();
    return m_can;
  }
  bool createPlots()
  {
    if( !layer ) layer = new TH1D( "layer", "layer", 3, 0, 2 );
    return layer;
  }
  TCanvas*        m_can{ nullptr };
  TH1D*           layer{ nullptr };
  std::thread     m_thread;
  RPCDataAnalyzer m_analyse;
};

int main( int argc, char* argv[] )
try
{
  TApplication rootApp( "ROOT", &argc, argv );
  //auto canvas = new TCanvas("c1", "Layer", 800, 600);
  //auto layer  = new TH1D("layer", "Layer", 3, 0, 3);

  //layer->Draw();
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
  Analyser module( cfg, "DCT" /*, canvas, layer */ );
  //client.setTLS("/home/work/YAODAQ-1/localhost.crt","/home/work/YAODAQ-1/localhost.key","NONE");
  module.link();
  std::size_t nbrCTLC{ 3 };
  Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press " << std::to_string( nbrCTLC ) << " times CTRL+C to stop" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
  while( true )
  {
    gSystem->ProcessEvents();
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
