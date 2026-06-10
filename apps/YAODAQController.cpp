#include <CLI/CLI.hpp>
#include <cpp-terminal/color.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <yaodaq/Controller.hpp>
#include <yaodaq/Exception.hpp>

int main( int argc, char* argv[] )
try
{
  Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
  CLI::App app{ "YAODAQ controller" };
  argv = app.ensure_utf8( argv );
  std::string name{ "controller" };
  app.add_option( "-n,--name", name, "Name of the client" );
  std::string host{ "127.0.0.1" };
  app.add_option( "-i,--ip", host, "IP of the server" ) /*->check( CLI::ValidIPV4 )*/;
  int port{ 8888 };
  app.add_option( "-p,--port", port, "Port to listen" )->check( CLI::Range( 0, 65535 ) );
  yaodaq::verbosity::level verbosity{ yaodaq::verbosity::level::info };
  app.add_option( "--verbosity", verbosity, "Verbosity" )->transform( CLI::CheckedTransformer( yaodaq::verbosity::map, CLI::ignore_case ) );

  try
  {
    app.parse( argc, argv );
  }
  catch( const CLI::ParseError& e )
  {
    return app.exit( e );
  }
  yaodaq::Config cfg;
  cfg().setPort( port ).setHost( host ).verbosity( verbosity );
  yaodaq::Controller controller( name, cfg );
  controller.link();
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
        else if( key == Term::Key::s )
        {
          Term::cout << controller.CallMethod( "getState" ).pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_I )
        {
          Term::cout << controller.initialize().tabulate() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_L )
        {
          Term::cout << controller.connect().tabulate() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_C )
        {
          Term::cout << controller.configure().tabulate() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_S )
        {
          Term::cout << controller.start().tabulate() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::p )
        {
          Term::cout << controller.pause().tabulate() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::r )
        {
          Term::cout << controller.resume().tabulate() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_K )
        {
          Term::cout << controller.stop().tabulate() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_Z )
        {
          Term::cout << controller.clear().tabulate() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_D )
        {
          Term::cout << controller.disconnect().tabulate() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_R )
        {
          Term::cout << controller.release().tabulate() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::h )
        {
          Term::cout << controller.CallMethod( "listProcedures" ).pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else
        {
          nbrCTLC = 3;
          Term::cout << "Press :\n";
          Term::cout << "h : list procedures\n";
          Term::cout << "s : list states\n";

          Term::cout << "Ctrl+I : initialize\n";
          Term::cout << "Ctrl+L : connect\n";
          Term::cout << "Ctrl+C : configure\n";
          Term::cout << "Ctrl+S : start\n";
          Term::cout << "p : pause\n";
          Term::cout << "r : resume\n";
          Term::cout << "Ctrl+K : stop\n";
          Term::cout << "Ctrl+Z : clear\n";
          Term::cout << "Ctrl+D : disconnect\n";
          Term::cout << "Ctrl+R : release\n" << std::endl;
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
  Term::cerr << Term::color_fg( Term::Color::Name::Red ) << "exception thrown" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
}
