#include <CLI/CLI.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <filesystem>
#include <yaodaq/Exception.hpp>
#include <yaodaq/Module.hpp>

class TDC : public yaodaq::Module
{
public:
  TDC( const std::string path ) : yaodaq::Module( "TDC" ), m_path( path ) {}
  virtual bool start()
  {
    std::string filePath = makeFileName( expectedIndex );
    if( fileExists( filePath ) )
    {
      std::ifstream file( m_path );
      if( !file.is_open() ) { std::cerr << "Failed to open: " << m_path << "\n"; }

      std::cout << "Processing: " << m_path << "\n";

      std::string line;
      while( std::getline( file, line ) )
      {
        // Process line (here we just print)
        std::cout << line << "\n";
      }

      file.close();

      // Delete file after processing
      std::filesystem::remove( filePath );
      std::cout << "Deleted: " << filePath << "\n";
      return true;
    }
    else
      std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
  }

private:
  std::string makeFileName( int index ) { return m_path + "/tmp_file_" + std::to_string( index ) + ".csv"; }

  bool             fileExists( const std::string& path ) { return std::filesystem::exists( path ); }
  std::string      m_path;
  std::atomic<int> expectedIndex;
  std::mutex       mtx;
};

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
  std::string path;
  app.add_option( "--path", path, "path to read the CSV" );

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
  module.link();

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
        if( key == Term::Key::Ctrl_Q )
        {
          ++nbrCTLC;
          if( nbrCTLC == 3 ) return 0;
        }
        else if( key == Term::Key::s )
        {
          nlohmann::json nbr = module.CallMethod( "getState" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::Ctrl_I )
        {
          nlohmann::json nbr = module.CallMethod( "initialize" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::Ctrl_C )
        {
          nlohmann::json nbr = module.CallMethod( "configure" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::Ctrl_S )
        {
          nlohmann::json nbr = module.CallMethod( "start" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::p )
        {
          nlohmann::json nbr = module.CallMethod( "pause" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::Ctrl_Z )
        {
          nlohmann::json nbr = module.CallMethod( "stop" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::Ctrl_K )
        {
          nlohmann::json nbr = module.CallMethod( "clear" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::Ctrl_R )
        {
          nlohmann::json nbr = module.CallMethod( "release" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::Ctrl_L )
        {
          nlohmann::json nbr = module.CallMethod( "connect" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::r )
        {
          nlohmann::json nbr = module.CallMethod( "resume" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::Ctrl_D )
        {
          nlohmann::json nbr = module.CallMethod( "disconnect" );
          std::cout << nbr.dump( 2 ) << std::endl;
        }
        else if( key == Term::Key::h ) { std::cout << module.CallMethod( "listProcedures" ).pretty_format() << std::endl; }
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
