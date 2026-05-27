#include "yaodaq/WebSocketTransport.hpp"

#include <CLI/CLI.hpp>
#include <chrono>
#include <cpp-terminal/color.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <iostream>
#include <ixwebsocket/IXHttpClient.h>
#include <simdjson.h>
#include <soci/mysql/soci-mysql.h>
#include <soci/soci.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <yaodaq/Board.hpp>
#include <yaodaq/Exception.hpp>

// -----------------------------
// Key: (board, channel)
// -----------------------------
struct Key
{
  int crate;
  int board;
  int chan;

  bool operator==( const Key& o ) const { return crate == o.crate && board == o.board && chan == o.chan; }
};

struct KeyHash
{
  size_t operator()( const Key& k ) const { return std::hash<int>()( k.crate ) ^ ( std::hash<int>()( k.board ) << 1 ) ^ ( std::hash<int>()( k.chan ) << 2 ); }
};

// -----------------------------
// Measurement row
// -----------------------------
struct Row
{
  std::string ts;
  long        channel_id;
  double      voltage;
  double      current;
};

struct Info
{
  std::string ts;
  double      voltage;
  double      current;
  int         crate;
  int         board;
  int         channel;
};

struct Sample
{
  std::string ts;

  double voltage = 0.0;
  double current = 0.0;

  bool has_v = false;
  bool has_i = false;
};

std::unordered_map<Key, long long, KeyHash> channel_map;

// -----------------------------
// DAQ Writer
// -----------------------------
class HVWriter
{
public:
  void          setDB( const std::string& db ) { m_db = db; }
  void          setUser( const std::string& user ) { m_user = user; }
  void          setPassword( const std::string& password ) { m_password = password; }
  void          setHost( const std::string& host ) { m_host = host; }
  void          setPort( const std::string& port ) { m_port = port; }
  soci::session sql;

  std::vector<Row> buffer;

  void connect() { sql.open( soci::mysql, fmt::format( "dbname={} user={} password={} host={} port={}", m_db, m_user, m_password, m_host, m_port ) ); }

  // -----------------------------
  // PRELOAD CHANNEL MAP
  // -----------------------------
  void preload()
  {
    soci::rowset<soci::row> rs = ( sql.prepare << "SELECT ch.channel_id, c.crate_no, b.board_addr, ch.hv_chan FROM hv_channel ch JOIN hv_board b ON ch.board_id = b.board_id JOIN crate c ON b.crate_id = c.crate_id" );

    for( auto it = rs.begin(); it != rs.end(); ++it )
    {
      soci::row const& r = *it;

      long long channel_id = r.get<long long>( 0 );
      int       crate      = r.get<int>( 1 );
      int       board      = r.get<int>( 2 );
      int       chan       = r.get<int>( 3 );
      std::cout << channel_id << " " << crate << " " << board << " " << chan << std::endl;
      channel_map[{ crate, board, chan }] = channel_id;
    }
  }

  // -----------------------------
  // ADD EVENT (DAQ STREAM)
  // -----------------------------
  void add( const std::string& ts, int crate, int board, int chan, double voltage, double current )
  {
    auto it = channel_map.find( { crate, board, chan } );
    if( it == channel_map.end() ) return;

    buffer.push_back( { ts, it->second, voltage, current } );

    if( buffer.size() >= 1 ) { flush(); }
  }

  // -----------------------------
  // FLUSH BATCH INSERT
  // -----------------------------
  void flush()
  {
    if( buffer.empty() ) return;

    soci::statement st = ( sql.prepare << "INSERT INTO hv_measurement "
                                          "(ts, channel_id, voltage_V, current_A) "
                                          "VALUES (:ts, :ch, :v, :c)" );

    for( auto& r: buffer )
    {
      st.exchange( soci::use( r.ts, "ts" ) );
      st.exchange( soci::use( r.channel_id, "ch" ) );
      st.exchange( soci::use( r.voltage, "v" ) );
      st.exchange( soci::use( r.current, "c" ) );

      st.define_and_bind();
      st.execute( true );
    }

    buffer.clear();
  }

private:
  std::string m_db;
  std::string m_user;
  std::string m_password;
  std::string m_host;
  std::string m_port;
};

class MotorBoard : public yaodaq::Board
{
public:
  MotorBoard( yaodaq::BoardConfig& cfg, const std::string_view name ) : yaodaq::Board( cfg, name, "MotorBoard" ) {}
  bool on_initialize() override
  {
    writer.setDB( "hv_monitor" );
    writer.setUser( "yaodaq" );
    writer.setPassword( "yaodaq" );
    writer.setHost( "192.168.50.18" );
    writer.setPort( "3306" );
    writer.connect();
    writer.preload();
    info( "Finding the key" );
    ix::HttpRequestArgsPtr args = m_http.createRequest();
    args->connectTimeout        = 1;
    args->transferTimeout       = 1;
    args->followRedirects       = true;
    ix::HttpResponsePtr out;
    std::string         url = "http://192.168.50.119:8081/api/login/admin/password";
    out                     = m_http.get( url, args );
    if( out->statusCode != 200 || out->body == "connection error" || out->body.empty() )
    {
      error( "error: {} ({})", out->body, out->statusCode );
      return false;
    }
    else
    {
      info( "{} {} ", out->body, out->statusCode );
    }
    nlohmann::json data = nlohmann::json::parse( out->body );
    m_key               = data["i"].get<std::string>();
    info( "Found key {}", m_key );
    return true;
  }

  bool on_connect() override
  {
    std::function<bool()> fun = [this]() -> bool
    {
      std::this_thread::sleep_for( std::chrono::seconds( 2 ) );

      nlohmann::json data;
      data["i"]                   = m_key.c_str();
      data["t"]                   = "request";
      data["r"]                   = "websocket";
      data["c"]                   = nlohmann::json::array();
      data["c"][0]["c"]           = "getItem";
      data["c"][0]["p"]["i"]      = "Status.currentMeasure";
      data["c"][0]["p"]["v"]      = "";
      data["c"][0]["p"]["u"]      = "";
      data["c"][0]["p"]["p"]["l"] = "*";
      data["c"][0]["p"]["p"]["a"] = "*";
      data["c"][0]["p"]["p"]["c"] = "*";

      data["c"][1]["c"]           = "getItem";
      data["c"][1]["p"]["i"]      = "Status.voltageMeasure";
      data["c"][1]["p"]["v"]      = "";
      data["c"][1]["p"]["u"]      = "";
      data["c"][1]["p"]["p"]["l"] = "*";
      data["c"][1]["p"]["p"]["a"] = "*";
      data["c"][1]["p"]["p"]["c"] = "*";
      send( yaodaq::Raw( data ) );

      return true;
    };

    this->setRun( fun );

    return true;
  }
  HVWriter writer;

private:
  std::string    m_key;
  ix::HttpClient m_http;
};

std::string microsecondsToUTC( std::string s )
{
  s.erase( std::remove( s.begin(), s.end(), '.' ), s.end() );
  std::cout << " val" << s << std::endl;
  using namespace std::chrono;
  // 1. Convert microseconds → system_clock time_point
  system_clock::time_point tp = system_clock::time_point( microseconds( std::stoll( s ) ) );

  // 2. Convert to time_t (seconds since epoch)
  std::time_t t = system_clock::to_time_t( tp );

  // 3. Convert to UTC struct
  std::tm utc_tm;

#if defined( _WIN32 )
  gmtime_s( &utc_tm, &t );  // Windows
#else
  gmtime_r( &t, &utc_tm );  // Linux / macOS
#endif

  // 4. Format output
  std::ostringstream oss;
  oss << std::put_time( &utc_tm, "%Y-%m-%d %H:%M:%S" );

  return oss.str();
}

std::vector<Info> parse( const std::string& str )
{
  std::cout << str << std::endl;
  static std::unordered_map<Key, Sample, KeyHash> sample_map;
  std::vector<Info>                               m_infos;
  simdjson::ondemand::parser                      parser;
  simdjson::padded_string                         padded( str );
  auto                                            doc  = parser.iterate( padded );
  auto                                            root = doc.get_array();

  for( auto response: root )
  {
    auto obj = response.get_object();
    if( obj["t"] != "response" ) continue;

    for( auto event: obj["c"] )
    {
      auto e = event.get_object();
      if( e["e"] != "itemUpdated" ) continue;

      auto d = e["d"];
      auto p = d["p"];

      int              crate = std::stoi( std::string( std::string_view( p["l"] ) ) );
      int              board = std::stoi( std::string( std::string_view( p["a"] ) ) );
      int              chan  = std::stoi( std::string( std::string_view( p["c"] ) ) );
      std::string_view item  = d["i"];
      double           value = std::stod( std::string( std::string_view( d["v"] ) ) );
      std::string      time  = std::string( std::string_view( d["t"] ) );
      std::cout << time << std::endl;
      Key key{ crate, board, chan };

      // ✔ IMPORTANT: reference, not copy
      Sample& s = sample_map[key];

      s.ts = microsecondsToUTC( time );

      // -----------------------------
      // UNIT NORMALIZATION
      // -----------------------------
      double v = value;

      std::string_view u = d["u"];

      if( u == "nA" ) v *= 1e-9;
      else if( u == "uA" )
        v *= 1e-6;
      else if( u == "mA" )
        v *= 1e-3;
      else if( u == "mV" )
        v *= 1e-3;
      else if( u == "uV" )
        v *= 1e-6;

      // -----------------------------
      // MERGE
      // -----------------------------
      if( item == "Status.currentMeasure" )
      {
        s.current = v;
        s.has_v   = true;
      }
      else if( item == "Status.voltageMeasure" )
      {
        s.voltage = v;
        s.has_i   = true;
      }

      // -----------------------------
      // COMPLETE SAMPLE
      // -----------------------------
      if( s.has_v && s.has_i )
      {
        Info info;
        info.ts      = s.ts;
        info.voltage = s.voltage;
        info.current = s.current;
        info.crate   = crate;
        info.board   = board;
        info.channel = chan;
        std::cout << info.ts << " " << info.voltage << " " << info.current << " " << info.crate << " " << info.board << " " << info.channel << std::endl;
        // reset AFTER extraction
        m_infos.push_back( info );
        s.has_v = false;
        s.has_i = false;
      }
    }
  }
  return m_infos;  // no complete sample
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
  json["url"] = "ws://192.168.50.119:8080";
  yaodaq::BoardConfig cfg( std::make_unique<Connector>( std::make_unique<WebSocket>( json ), std::make_unique<yaodaq::Json>() ) );
  cfg().setPort( port ).setHost( host ).verbosity( verbosity );

  MotorBoard board( cfg, name );
  board.dispatcher().subscribeToAll(
    [&board]( const yaodaq::Message& msg ) -> void
    {
      std::vector<Info> info = parse( msg.payload().dump() );
      for( std::size_t i = 0; i != info.size(); ++i ) std::cout << fmt::format( "time: {} crate: {} board: {}, channel:{} voltage:{} current:{}", info[i].ts, info[i].crate, info[i].board, info[i].channel, info[i].voltage, info[i].current ) << std::endl;

      //board.writer.add(info.ts,info.crate,info.board,info.channel,info.voltage, info.current);
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
