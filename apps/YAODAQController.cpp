#include <CLI/CLI.hpp>
#include <algorithm>
#include <cctype>
#include <cpp-terminal/color.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <sstream>
#include <yaodaq/Controller.hpp>
#include <yaodaq/Exception.hpp>
void callMethod( yaodaq::Controller& controller )
{
  Term::cout << "\n"
             << "================ Method mode ================\n"
             << "Call any JSON-RPC method using:\n"
             << "  method argument argument ...\n"
             << "\nExamples:\n"
             << "  getState\n"
             << "  initialize\n"
             << "  configure physics 42\n"
             << "  setThreshold 0.5\n"
             << "  setEnabled true\n"
             << "  setFileName myfile.root\n"
             << "  setFileName \"my file.root\"\n"
             << "=============================================\n"
             << "Commands:\n"
             << "  quit / exit : leave method mode\n"
             << "  ESC         : leave method mode\n"
             << "  CTRL+C      : leave method mode\n"
             << "=============================================\n";

  auto readLine = []() -> std::optional<std::string>
  {
    std::string line;

    Term::cout << "\n> " << std::flush;

    while( true )
    {
      Term::Event event = Term::read_event();

      if( event.type() != Term::Event::Type::Key ) continue;

      Term::Key key( event );

      if( key == Term::Key::Enter )
      {
        Term::cout << '\n';
        return line;
      }

      if( key == Term::Key::Backspace )
      {
        if( !line.empty() )
        {
          line.pop_back();
          Term::cout << "\b \b" << std::flush;
        }

        continue;
      }

      if( key == Term::Key::Esc || key == Term::Key::Ctrl_C )
      {
        Term::cout << '\n';
        return std::nullopt;
      }

      const std::string character = key.str();

      if( !character.empty() && std::all_of( character.begin(), character.end(), []( unsigned char c ) { return std::isprint( c ); } ) )
      {
        line += character;
        Term::cout << character << std::flush;
      }
    }
  };

  /*
        Split command line while preserving quoted strings.

        Example:

            setFileName "my file.root"

        becomes

            [
                "setFileName",
                "my file.root"
            ]
    */
  auto tokenize = []( const std::string& input )
  {
    std::vector<std::string> tokens;

    std::string token;
    bool        quoted = false;

    for( char c: input )
    {
      if( c == '"' )
      {
        quoted = !quoted;
        continue;
      }

      if( std::isspace( static_cast<unsigned char>( c ) ) && !quoted )
      {
        if( !token.empty() )
        {
          tokens.push_back( token );
          token.clear();
        }
      }
      else
      {
        token += c;
      }
    }

    if( !token.empty() ) tokens.push_back( token );

    return tokens;
  };

  /*
        Convert a command-line argument into the most appropriate JSON type.

        Examples:

            "42"          -> 42
            "-5"          -> -5
            "3.14"        -> 3.14
            "true"        -> true
            "false"       -> false
            "null"        -> null
            "[1,2]"       -> [1,2]
            "{\"a\":1}"   -> {"a":1}
            "file.root"   -> "file.root"
    */
  auto parseArgument = []( const std::string& value ) -> nlohmann::json
  {
    if( value == "true" ) return true;

    if( value == "false" ) return false;

    if( value == "null" ) return nullptr;

    try
    {
      std::size_t pos = 0;
      long long   v   = std::stoll( value, &pos );

      if( pos == value.size() ) return v;
    }
    catch( ... )
    {
    }

    try
    {
      std::size_t        pos = 0;
      unsigned long long v   = std::stoull( value, &pos );

      if( pos == value.size() ) return v;
    }
    catch( ... )
    {
    }

    try
    {
      std::size_t pos = 0;
      double      v   = std::stod( value, &pos );

      if( pos == value.size() ) return v;
    }
    catch( ... )
    {
    }

    if( !value.empty() && ( value.front() == '[' || value.front() == '{' ) )
    {
      try
      {
        return nlohmann::json::parse( value );
      }
      catch( ... )
      {
      }
    }

    return value;
  };

  while( true )
  {
    auto line = readLine();

    if( !line )
    {
      Term::cout << "Leaving method mode\n";
      return;
    }

    if( line->empty() ) continue;

    if( *line == "quit" || *line == "exit" )
    {
      Term::cout << "Leaving method mode\n";
      return;
    }

    try
    {
      auto tokens = tokenize( *line );

      if( tokens.empty() ) continue;

      const std::string method = tokens.front();

      nlohmann::json params = nlohmann::json::array();

      for( size_t i = 1; i < tokens.size(); ++i ) params.push_back( parseArgument( tokens[i] ) );

      Term::cout << "Calling " << method << " with " << params.dump( 2 ) << '\n';

      yaodaq::Response response = params.empty() ? controller.CallMethod( method ) : controller.CallMethod( method, params );

      Term::cout << response.tabulate() << '\n';
    }
    catch( const jsonrpc::exception& e )
    {
      Term::cerr << "JSON-RPC error: " << e.what() << '\n';
    }
    catch( const std::exception& e )
    {
      Term::cerr << "Error: " << e.what() << '\n';
    }
  }
}

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
  cfg.setPort( port ).setHost( host ).verbosity( verbosity );
  yaodaq::Controller controller( name, cfg );
  controller.link();
  std::size_t nbrCTLC{ 3 };
  Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press " << std::to_string( nbrCTLC ) << " times CTRL+C to stop" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
  while( true )
  {
    try
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
            Term::cout << controller.CallMethod( "getState" ).tabulate() << std::endl;
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
            Term::cout << controller.CallMethod( "listProcedures" ).tabulate() << std::endl;
            nbrCTLC = 3;
          }
          else if( key == Term::Key::m )
          {
            callMethod( controller );
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
    }
    catch( const jsonrpc::exception& exception )
    {
      if( exception.code() == jsonrpc::timeout ) { Term::cerr << exception.what() << std::endl; }
      else
        throw;
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
