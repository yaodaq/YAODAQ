#include <CLI/CLI.hpp>
#include <cpp-terminal/color.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <yaodaq/Module.hpp>
#include <filesystem>
#include <chrono>
#include "fmt/chrono.h"
#include "fmt/std.h"
#include <memory>
#include "RawData.hpp"
#include "Data.hpp"
#include <TBufferJSON.h>


class Analyser : public yaodaq::Module
{
public:
  Analyser( yaodaq::Config cfg, const std::string_view name ) : yaodaq::Module( cfg, "MyLovelyAnalyser", "Analyser" )
  {
  }
  ~Analyser() override
  {
  }
  void onRawData( const std::unique_ptr<yaodaq::RawData> raw ) override
  {
    
    if(raw->topic()=="MPI::DCT::Singlets::Events")
    {
    
    std::string raw2(reinterpret_cast<const char*>(raw->payload().data()),raw->payload().size());
    auto obj = TBufferJSON::FromJSON<DCT::Event>(raw2);
        info("Event {}",obj->event_number);
        for(std::size_t i=0; i!=obj->hits.size();++i) warn("layer: {}, side: {}, strip: {}, rise: {}",obj->hits[i].getLayer(), obj->hits[i].getSide(), obj->hits[i].getStrip(),obj->hits[i].getRise());
        warn("{} trigger hits, {} hits",obj->trigger_hits.size(),obj->hits.size());
        info("END Event {}",obj->event_number);
     ++m_event;
    }   
  }
private:
  std::uint64_t m_event{0};
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
  Analyser module( cfg, "DCT" );
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
