
#include "yaodaq/WebSocketTransport.hpp"

#include <CLI/CLI.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cpp-terminal/color.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <iostream>
#include <ixwebsocket/IXHttpClient.h>
#include <mutex>
#include <simdjson.h>
#include <soci/mysql/soci-mysql.h>
#include <soci/soci.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <yaodaq/Board.hpp>
#include <yaodaq/Exception.hpp>
#include <yaodaq/Formatter.hpp>

// ============================================================
// Key
// ============================================================

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

// ============================================================
// Measurement row
// ============================================================

struct Row
{
  std::string ts;
  long long   channel_id;
  double      voltage;
  double      current;
};

struct Info
{
  std::string ts;

  double voltage;
  double current;

  int crate;
  int board;
  int channel;
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

// ============================================================
// HVWriter
// ============================================================

class HVWriter
{
public:
  soci::session sql;

  void setDB( const std::string& db ) { m_db = db; }

  void setUser( const std::string& user ) { m_user = user; }

  void setPassword( const std::string& password ) { m_password = password; }

  void setHost( const std::string& host ) { m_host = host; }

  void setPort( const std::string& port ) { m_port = port; }

  void connect() { sql.open( soci::mysql, fmt::format( "dbname={} user={} password={} host={} port={}", m_db, m_user, m_password, m_host, m_port ) ); }

  // ========================================================
  // PRELOAD CHANNEL MAP
  // ========================================================

  void preload()
  {
    soci::rowset<soci::row> rs = ( sql.prepare << "SELECT "
                                                  "ch.channel_id, "
                                                  "c.crate_no, "
                                                  "b.board_addr, "
                                                  "ch.hv_chan "
                                                  "FROM hv_channel ch "
                                                  "JOIN hv_board b "
                                                  "ON ch.board_id = b.board_id "
                                                  "JOIN crate c "
                                                  "ON b.crate_id = c.crate_id" );

    for( auto it = rs.begin(); it != rs.end(); ++it )
    {
      soci::row const& r = *it;

      long long channel_id = r.get<long long>( 0 );
      int       crate      = r.get<int>( 1 );
      int       board      = r.get<int>( 2 );
      int       chan       = r.get<int>( 3 );

      channel_map[{ crate, board, chan }] = channel_id;
    }
  }

  // ========================================================
  // START DB THREAD
  // ========================================================

  void start()
  {
    m_running = true;

    m_thread = std::thread( [this]() { run(); } );
  }

  // ========================================================
  // STOP DB THREAD
  // ========================================================

  void stop()
  {
    m_running = false;

    m_cv.notify_all();

    if( m_thread.joinable() ) m_thread.join();
  }

  // ========================================================
  // ADD EVENT
  // ========================================================

  void add( const std::string& ts, int crate, int board, int chan, double voltage, double current )
  {
    auto it = channel_map.find( { crate, board, chan } );

    if( it == channel_map.end() ) return;

    {
      std::lock_guard<std::mutex> lock( m_mutex );

      buffer.push_back( { ts, it->second, voltage, current } );
    }

    if( buffer.size() >= 6 ) m_cv.notify_one();
  }

private:
  // ========================================================
  // DB THREAD LOOP
  // ========================================================

  void run()
  {
    while( m_running )
    {
      {
        std::unique_lock<std::mutex> lock( m_mutex );

        m_cv.wait_for( lock, std::chrono::seconds( 1 ), [this]() { return !buffer.empty() || !m_running; } );
      }

      flush();
    }

    flush();
  }

  // ========================================================
  // FLUSH
  // ========================================================

  void flush()
  {
    std::vector<Row> local;

    {
      std::lock_guard<std::mutex> lock( m_mutex );

      if( buffer.empty() ) return;

      local.swap( buffer );
    }

    try
    {
      soci::statement st = ( sql.prepare << "INSERT IGNORE INTO hv_measurement "
                                            "(ts, channel_id, voltage_V, current_A) "
                                            "VALUES (:ts, :ch, :v, :c)" );

      for( auto& r: local )
      {
        st.exchange( soci::use( r.ts, "ts" ) );
        st.exchange( soci::use( r.channel_id, "ch" ) );
        st.exchange( soci::use( r.voltage, "v" ) );
        st.exchange( soci::use( r.current, "c" ) );

        st.define_and_bind();
        st.execute( true );
      }
    }
    catch( const std::exception& e )
    {
      std::cerr << "DB flush exception: " << e.what() << std::endl;
    }
  }

private:
  std::string m_db;
  std::string m_user;
  std::string m_password;
  std::string m_host;
  std::string m_port;

  std::mutex              m_mutex;
  std::condition_variable m_cv;

  std::vector<Row> buffer;

  std::thread m_thread;

  std::atomic<bool> m_running{ false };
};

// ============================================================
// MotorBoard
// ============================================================

class MotorBoard : public yaodaq::Board
{
public:
  MotorBoard( yaodaq::BoardConfig& cfg, const std::string_view name ) : yaodaq::Board( cfg, name, "MotorBoard" ) {}

  ~MotorBoard() { writer.stop(); }

  bool on_initialize() override
  {
    writer.setDB( "hv_monitor" );
    writer.setUser( "yaodaq" );
    writer.setPassword( "yaodaq" );
    writer.setHost( "192.168.50.18" );
    writer.setPort( "3306" );

    writer.connect();
    writer.preload();
    writer.start();

    info( "Finding the key" );

    ix::HttpRequestArgsPtr args = m_http.createRequest();

    args->connectTimeout  = 1;
    args->transferTimeout = 1;
    args->followRedirects = true;

    std::string url = "http://192.168.50.119:8081/api/login/admin/password";

    ix::HttpResponsePtr out = m_http.get( url, args );

    if( out->statusCode != 200 || out->body == "connection error" || out->body.empty() )
    {
      error( "error: {} ({})", out->body, out->statusCode );

      return false;
    }

    out->body.erase( std::remove_if( out->body.begin(), out->body.end(), []( unsigned char c ) { return std::isspace( c ); } ), out->body.end() );

    m_key = out->body;

    info( "Found key *{}*", out->body );

    return true;
  }

  bool on_connect() override
  {
    std::function<bool()> fun = [this]() -> bool
    {
      std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );

      nlohmann::json json{ { "i", m_key },
                           { "t", "request" },
                           { "c", nlohmann::json::array( { { { "c", "getItem" }, { "p", { { "p", { { "l", "0" }, { "a", "*" }, { "c", "*" } } }, { "i", "Status.currentMeasure" }, { "v", "" }, { "u", "" } } } },
                                                           { { "c", "getItem" }, { "p", { { "p", { { "l", "0" }, { "a", "*" }, { "c", "*" } } }, { "i", "Status.voltageMeasure" }, { "v", "" }, { "u", "" } } } } } ) },
                           { "r", "xhr" } };

      yaodaq::Raw raw( json );

      send( raw );

      return true;
    };

    this->setRun( fun );

    return true;
  }

  HVWriter writer;

private:
  std::string m_key;

  ix::HttpClient m_http;
};

// ============================================================
// FORMAT UTC
// ============================================================

std::string format_utc( const std::string_view ts_str )
{
  using namespace std::chrono;

  double ts = std::stod( ts_str.data() );

  auto tp = time_point<system_clock, microseconds>( duration_cast<microseconds>( duration<double>( ts ) ) );

  auto sec_tp = time_point_cast<seconds>( tp );

  std::time_t tt = system_clock::to_time_t( sec_tp );

  std::tm tm_utc;

#if defined( _WIN32 )
  gmtime_s( &tm_utc, &tt );
#else
  gmtime_r( &tt, &tm_utc );
#endif

  auto us = duration_cast<microseconds>( tp.time_since_epoch() ) % 1000000;

  return fmt::format( "{:04d}-{:02d}-{:02d} "
                      "{:02d}:{:02d}:{:02d}.{:06d}",
                      tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday, tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec, (int)us.count() );
}

// ============================================================
// PARSE
// ============================================================

std::vector<Info> parse( const std::string& str )
{
  static std::unordered_map<Key, Sample, KeyHash> sample_map;

  std::vector<Info> m_infos;

  simdjson::ondemand::parser parser;

  simdjson::padded_string padded( str );

  auto doc  = parser.iterate( padded );
  auto root = doc.get_array();

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

      int crate = std::stoi( std::string( std::string_view( p["l"] ) ) );

      int board = std::stoi( std::string( std::string_view( p["a"] ) ) );

      int chan = std::stoi( std::string( std::string_view( p["c"] ) ) );

      std::string_view item = d["i"];

      double value = std::stod( std::string( std::string_view( d["v"] ) ) );

      std::string time = std::string( std::string_view( d["t"] ) );

      Key key{ crate, board, chan };

      Sample& s = sample_map[key];

      s.ts = format_utc( time );

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

      if( item == "Status.currentMeasure" )
      {
        s.current = v;
        s.has_i   = true;
      }
      else if( item == "Status.voltageMeasure" )
      {
        s.voltage = v;
        s.has_v   = true;
      }

      if( s.has_v && s.has_i )
      {
        Info info;

        info.ts      = s.ts;
        info.voltage = s.voltage;
        info.current = s.current;

        info.crate   = crate;
        info.board   = board;
        info.channel = chan;

        m_infos.push_back( info );

        s.has_v = false;
        s.has_i = false;
      }
    }
  }

  return m_infos;
}

// ============================================================
// MAIN
// ============================================================

int main( int argc, char* argv[] )
try
{
  Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );

  CLI::App app{ "YAODAQ module" };

  argv = app.ensure_utf8( argv );

  std::string name{ "board" };

  app.add_option( "-n,--name", name, "Name of the client" );

  std::string host{ "127.0.0.1" };

  app.add_option( "-i,--ip", host, "IP of the server" );

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
    [&board]( const yaodaq::Message& msg )
    {
      std::vector<Info> info = parse( msg.payload().dump() );

      for( std::size_t i = 0; i != info.size(); ++i ) { board.writer.add( info[i].ts, info[i].crate, info[i].board, info[i].channel, info[i].voltage, info[i].current ); }
    } );

  board.link();

  std::size_t nbrCTLC{ 3 };

  Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press " << std::to_string( nbrCTLC ) << " times CTRL+Q to stop" << Term::color_fg( Term::Color::Name::Default ) << std::endl;

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
  }

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
