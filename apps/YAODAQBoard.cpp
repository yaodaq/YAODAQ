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
#include <simdjson.h>
class MotorBoard : public yaodaq::Board
{
public:
  MotorBoard( yaodaq::BoardConfig& cfg, const std::string_view name ) : yaodaq::Board( cfg, name, "MotorBoard" ) {}
  bool on_initialize() override
  {
    info("Finding the key");
    ix::HttpRequestArgsPtr args = m_http.createRequest();
    args->connectTimeout        = 5;
    args->transferTimeout       = 2;
    args->followRedirects       = 1;
    ix::HttpResponsePtr out;
    std::string         url = "http://192.168.50.120/api/login/admin/password";
    out                     = m_http.get( url, args );
    if( out->statusCode != 200 || out->body == "connection error" )
    {
      error("error: {} ({})",out->body,out->statusCode);
      return false;
    }
    nlohmann::json data = nlohmann::json::parse( out->body );
    m_key               = data["i"].get<std::string>();
    return true;
  }


  bool on_connect() override
  {


    std::function<bool()> fun = [this]() -> bool
    {
      info("asking voltage/current");
      std::this_thread::sleep_for( std::chrono::seconds( 2 ) );

        nlohmann::json data;
    data["i"] = m_key.c_str();
    data["t"] = "request";
    data["r"] = "websocket";
    data["c"]= nlohmann::json::array();
    data["c"][0]["c"] = "getItem";
    data["c"][0]["p"]["i"] = "Status.currentMeasure";
    data["c"][0]["p"]["v"] = "";
    data["c"][0]["p"]["u"] = "";
    data["c"][0]["p"]["p"]["l"]= "*";
    data["c"][0]["p"]["p"]["a"]= "*";
    data["c"][0]["p"]["p"]["c"]= "*";

    data["c"][1]["c"] = "getItem";
    data["c"][1]["p"]["i"] = "Status.voltageMeasure";
    data["c"][1]["p"]["v"] = "";
    data["c"][1]["p"]["u"] = "";
    data["c"][1]["p"]["p"]["l"]= "*";
    data["c"][1]["p"]["p"]["a"]= "*";
    data["c"][1]["p"]["p"]["c"]= "*";
      send( yaodaq::Raw( data ) );

      return true;
    };

    this->setRun( fun );

    return true;
  }

private:
  std::string    m_key;
  ix::HttpClient m_http;
};


void parse(const std::string& json)
{
  simdjson::ondemand::parser parser;
  simdjson::padded_string padded(json);

  auto doc  = parser.iterate(padded);
  auto root = doc.get_array();

  for (auto response : root)
  {
        auto obj = response.get_object();

        if (obj["t"] != "response")
            continue;

        for (auto event : obj["c"])
        {
            auto e = event.get_object();

            if (e["e"] != "itemUpdated")
                continue;

            auto d = e["d"];
            auto p = d["p"];

            int line = std::string_view(p["l"]).empty() ? -1 : std::stoi(std::string(std::string_view(p["l"])));
            int addr = std::stoi(std::string(std::string_view(p["a"])));
            int chan = std::stoi(std::string(std::string_view(p["c"])));

            std::string_view item = d["i"];
            double value = std::stod(std::string(std::string_view(d["v"])));
            std::string_view unit = d["u"];
            std::string_view time = d["t"];

            std::cout
                << "L:" << line
                << " A:" << addr
                << " C:" << chan
                << " Item:" << item
                << " Value:" << value
                << " " << unit
                << " T:" << time
                << "\n";
        }
    }
}


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
  json["url"] = "ws://192.168.50.120:8080";
  yaodaq::BoardConfig cfg( std::make_unique<Connector>( std::make_unique<WebSocket>( json ), std::make_unique<yaodaq::Json>() ) );
  cfg().setPort( port ).setHost( host ).verbosity( verbosity );

  MotorBoard board( cfg, name );
  board.dispatcher().subscribeToAll([&board]( const yaodaq::Message& msg )->void
  {
    parse(msg.payload().dump());
    //std::cout<<yaodaq::Formatter::format(msg())<<std::endl;

  }

);
  board.link();

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
