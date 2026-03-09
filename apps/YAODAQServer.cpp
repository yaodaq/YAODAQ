#include <CLI/CLI.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <spdlog/spdlog.h>
#include <yaodaq/Exception.hpp>
#include <yaodaq/Server.hpp>

int main( int argc, char** argv )
try
{
  Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
  CLI::App app{ "YAODAQ server" };
  argv = app.ensure_utf8( argv );
  int port{ 8888 };
  app.add_option( "-p,--port", port, "Port to listen" )->check( CLI::Range( 0, 65535 ) );
  std::string host{ "127.0.0.1" };
  app.add_option( "-i,--ip", host, "IP of the server" )->check( CLI::ValidIPV4 );
  std::string name{ "server" };
  app.add_option( "-n,--name", name, "Name of the server" );
  app.parse( argc, argv );

  yaodaq::Server server( name, host, port );
  //server.rejectBrowsers();
  //server.setTLS("/home/work/YAODAQ-1/localhost.crt","/home/work/YAODAQ-1/localhost.key","/home/work/YAODAQ-1/RootCA.pem");
  server.loop();
  Term::cout << "Running " << server.identifier().id() << std::endl;
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
        if( key == Term::Key::Ctrl_C )
        {
          ++nbrCTLC;
          if( nbrCTLC == 3 ) return 0;
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
  spdlog::error( "{}", exception.what() );
}
catch( const CLI::ParseError& exception )
{
  spdlog::error( "{}", exception.what() );
  return exception.get_exit_code();
}
catch( const std::exception& exception )
{
  spdlog::error( "{}", exception.what() );
}
catch( ... )
{
  spdlog::error( "Exception thrown" );
}
