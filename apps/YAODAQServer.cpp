#include <CLI/CLI.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <limits>
#include <spdlog/spdlog.h>
#include <yaodaq/Exception.hpp>
#include <yaodaq/Server.hpp>

int main( int argc, char** argv )
try
{
  Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
  CLI::App app{ "YAODAQ server" };
  argv = app.ensure_utf8( argv );
  std::string name{ "server" };
  app.add_option( "-n,--name", name, "Name of the server" );
  std::string host{ "127.0.0.1" };
  app.add_option( "-i,--ip", host, "IP of the server" ) /*->check( CLI::ValidIPV4)*/;
  int port{ 8888 };
  app.add_option( "-p,--port", port, "Port to listen" )->check( CLI::Range( 0, 65535 ) );
  std::size_t maxConnections{ ( std::numeric_limits<std::size_t>::max )() };
  app.add_option( "-m,--maxConnections", maxConnections, "Maximum number of connection" )->check( CLI::Range( static_cast<std::size_t>( 1 ), ( std::numeric_limits<std::size_t>::max )() ) );
  int handshakeTimeoutSecs{ 1 };
  app.add_option( "-a,--hanshake_timeout", handshakeTimeoutSecs, "Handshake timeout (s)" )->check( CLI::Range( 1, ( std::numeric_limits<int>::max )() ) );
  int pingIntervalSeconds{ -1 };
  app.add_option( "-k,--ping_interval", pingIntervalSeconds, "Ping interval (s)" )->check( CLI::Range( -1, ( std::numeric_limits<int>::max )() ) );
  int backlog{ ( std::numeric_limits<int>::max )() };
  app.add_option( "-t,--tcp_backlog", backlog, "TCP backlog" )->check( CLI::Range( 1, ( std::numeric_limits<int>::max )() ) );
  int addressFamily{};
  app.add_option( "-f,--address_family", addressFamily, "Address family" )->check( CLI::Range( 1, ( std::numeric_limits<int>::max )() ) );
  bool rejectBrowsers{ false };
  app.add_flag( "-r", rejectBrowsers, "Reject Browsers" );
  try
  {
    app.parse( argc, argv );
  }
  catch( const CLI::ParseError& e )
  {
    return app.exit( e );
  }

  yaodaq::Server server( name, host, port, maxConnections, handshakeTimeoutSecs, 150, addressFamily );
  if( rejectBrowsers ) server.rejectBrowsers();
  //server.setTLS("/home/work/YAODAQ-1/localhost.crt","/home/work/YAODAQ-1/localhost.key","/home/work/YAODAQ-1/RootCA.pem");
  server.start();
  Term::cout << "Press 3 times CTRL+C to stop" << std::endl;
  Term::cout << "This server understand these procedures : " << std::endl;
  Term::cout << server.getProcedures() << std::endl;
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
        else if( key == Term::Key::p ) { std::cout << server.CallMethod( "listProcedures" ).pretty_format() << std::endl; }
        else if( key == Term::Key::i ) { std::cout << server.CallMethod( "listProcedures555" ).pretty_format() << std::endl; }
        break;
      }
      default: break;
    }
  };
  return 0;
}
catch( const yaodaq::Exception& exception )
{
  spdlog::error( "yaodaq: {}", exception.what() );
}
catch( const std::exception& exception )
{
  spdlog::error( "std::exception {}", exception.what() );
}
catch( ... )
{
  spdlog::error( "Exception thrown" );
}
