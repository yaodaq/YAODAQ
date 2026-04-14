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
  yaodaq::Controller controller( name, cfg );
  //client.setTLS("/home/work/YAODAQ-1/localhost.crt","/home/work/YAODAQ-1/localhost.key","NONE");
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
          std::cout << controller.CallMethod( "getState" ).pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_I )
        {
          std::cout << controller.initialize().pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_L )
        {
          std::cout << controller.connect().pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_C )
        {
          std::cout << controller.configure().pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_S )
        {
          std::cout << controller.start().pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::p )
        {
          std::cout << controller.pause().pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::r )
        {
          std::cout << controller.resume().pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_K )
        {
          std::cout << controller.stop().pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_Z )
        {
          std::cout << controller.clear().pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_D )
        {
          std::cout << controller.disconnect().pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::Ctrl_R )
        {
          std::cout << controller.release().pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else if( key == Term::Key::h )
        {
          std::cout << controller.CallMethod( "listProcedures" ).pretty_format() << std::endl;
          nbrCTLC = 3;
        }
        else
        {
          nbrCTLC = 3;
          Term::cout << "Press :" << std::endl;
          Term::cout << "h : list procedures" << std::endl;
          Term::cout << "s : list states" << std::endl;

          Term::cout << "Ctrl+I : initialize" << std::endl;
          Term::cout << "Ctrl+L : connect" << std::endl;
          Term::cout << "Ctrl+C : configure" << std::endl;
          Term::cout << "Ctrl+S : start" << std::endl;
          Term::cout << "p : pause" << std::endl;
          Term::cout << "r : resume" << std::endl;
          Term::cout << "Ctrl+K : stop" << std::endl;
          Term::cout << "Ctrl+Z : clear" << std::endl;
          Term::cout << "Ctrl+D : disconnect" << std::endl;
          Term::cout << "Ctrl+R : release" << std::endl;
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
