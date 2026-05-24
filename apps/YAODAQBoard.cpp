#include "yaodaq/WebSocketTransport.hpp"

#include <CLI/CLI.hpp>
#include <chrono>
#include <cpp-terminal/color.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <ixwebsocket/IXHttpClient.h>
#include <thread>
#include <yaodaq/Board.hpp>
#include <yaodaq/Exception.hpp>
class MotorBoard : public yaodaq::Board
{
public:
  MotorBoard( yaodaq::BoardConfig& cfg, const std::string_view name ) : yaodaq::Board( cfg, name, "MotorBoard" ) {}

  bool on_connect() override
  {
    ix::HttpRequestArgsPtr args = m_http.createRequest();
    args->connectTimeout        = 1;
    args->transferTimeout       = 1;
    args->followRedirects       = 1;
    ix::HttpResponsePtr out;
    std::string         url = "http://192.168.50.119/api/login/DAQ/daqdaq";
    out                     = m_http.get( url, args );
    if( out->statusCode != 200 || out->body == "connection error" ) { return false; }
    nlohmann::json data = nlohmann::json::parse( out->body );
    m_key               = data["i"];

    std::function<bool()> fun = [this]() -> bool
    {
      std::this_thread::sleep_for( std::chrono::seconds( 2 ) );
      nlohmann::json request;

      request["i"] = std::string( m_key );
      request["t"] = "request";
      request["r"] = "websocket";

      request["c"] = nlohmann::json::array();

      nlohmann::json current;
      current["c"] = "getItem";

      current["p"]["p"]["l"] = "*";
      current["p"]["p"]["a"] = "*";
      current["p"]["p"]["c"] = "*";

      current["p"]["i"] = "Status.currentMeasure";
      current["p"]["v"] = "";
      current["p"]["u"] = "";

      nlohmann::json voltage;
      voltage["c"] = "getItem";

      voltage["p"]["p"]["l"] = "*";
      voltage["p"]["p"]["a"] = "*";
      voltage["p"]["p"]["c"] = "*";

      voltage["p"]["i"] = "Status.voltageMeasure";
      voltage["p"]["v"] = "";
      voltage["p"]["u"] = "";

      request["c"].push_back( current );
      request["c"].push_back( voltage );

      send( yaodaq::Raw( request ) );

      return true;
    };

    this->setRun( fun );

    return true;
  }

private:
  std::string    m_key;
  ix::HttpClient m_http;
};

int main( int argc, char* argv[] )
try
{
  Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
  CLI::App app{ "YAODAQ module" };
  argv = app.ensure_utf8( argv );
  std::string name{ "board" };
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

  nlohmann::json json;
  json["url"] = "ws://192.168.50.119:8080";
  yaodaq::BoardConfig cfg( std::make_unique<Connector>( std::make_unique<WebSocket>( json ), std::make_unique<yaodaq::Json>() ) );
  cfg().setPort( port ).setHost( host ).verbosity( verbosity );

  MotorBoard board( cfg, name );
  board.link();
  board.dispatcher().subscribeToAll( [&board]( const yaodaq::Message& msg ) -> void { board.info( "received:\n{}\n", yaodaq::Formatter::format( msg() ) ); } );

  std::size_t nbrCTLC{ 3 };
  Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press " << std::to_string( nbrCTLC ) << " times CTRL+C to stop" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
  while( true )
  {
    Term::Event event = Term::read_event();
    switch( auto key = event.type() )
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
