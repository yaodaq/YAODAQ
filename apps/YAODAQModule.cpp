#include <CLI/CLI.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <yaodaq/Exception.hpp>
#include <yaodaq/Module.hpp>

int main( int argc, char* argv[] )
try
{
  Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
  CLI::App app{ "YAODAQ client" };
  argv = app.ensure_utf8( argv );
  std::string name{ "client" };
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
  yaodaq::Module module( name, host, port );
  //client.setTLS("/home/work/YAODAQ-1/localhost.crt","/home/work/YAODAQ-1/localhost.key","NONE");
  module.start();

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
        else if( key == Term::Key::i )
        {
          nlohmann::json nbr = module.CallMethod( "getNumberOfClients" );
          std::cout << "They are " << nbr.dump( 2 ) << " clients connected " << std::endl;
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
