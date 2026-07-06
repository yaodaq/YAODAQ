#include <CLI/CLI.hpp>
#include <cpp-terminal/color.hpp>
#include <cpp-terminal/input.hpp>
#include <cpp-terminal/iostream.hpp>
#include <cpp-terminal/terminal.hpp>
#include <yaodaq/Module.hpp>
#include "TFile.h"
#include <filesystem>
#include <chrono>
#include "fmt/chrono.h"
#include "fmt/std.h"
#include <memory>
#include "TROOT.h"
#include <ROOT/RNTupleWriter.hxx>
#include "RawData.hpp"
#include "Data.hpp"
#include <TBufferJSON.h>

struct TFileDeleter
{
  void operator()(TFile* f) const
  {
    if(!f) return;
    if(f->IsOpen()) f->Close();
    delete f;
  }
};
using TFilePtr = std::unique_ptr<TFile, TFileDeleter>;

class FileWriter : public yaodaq::Module
{
public:
  FileWriter( yaodaq::Config cfg, const std::string_view name ) : yaodaq::Module( cfg, "DCT", "FileWriter" )
  {
    ROOT::Experimental::DisableObjectAutoRegistration();
    ROOT::EnableImplicitMT();
    m_model= std::move(ROOT::RNTupleModel::Create());
    m_model_decoded = std::move(ROOT::RNTupleModel::Create());
    m_model_event = std::move(ROOT::RNTupleModel::Create());
    Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor ); //ROOT is doing bad stufs
  }
  ~FileWriter() override
  {
  }
  bool on_initialize() override
  {
    m_word = m_model->MakeField<std::vector<std::uint32_t>>("word");
    if(!m_word)
    {
        error("m_model->MakeField<std::uint32_t>(\"word\") failed");
        return false;
    }
    m_bcid = m_model->MakeField<std::vector<std::uint32_t>>("bcid");
    if(!m_bcid)
    {
        error("m_model->MakeField<std::uint32_t>(\"bcid\") failed");
        return false;
    }

    m_event = m_model_decoded->MakeField<DCT::PreProcessedEvent>("event");
    if(!m_event)
    {
      error("m_model_decoded->MakeField<std::vector<std::uint32_t>>(\"event\") failed");
      return false;
    }

    m_real_event = m_model_event->MakeField<DCT::Event>("event");
    if(!m_real_event)
    {
      error("m_real_event = m_model_event->MakeField<DCT::Event>(\"event\");");
      return false;
    }

    return true;
  }

  bool on_stop() override
  {
    info("Closing m_writer");
    if(m_writer)
    {
      m_writer.reset(nullptr);
    }
    else error("Writer is nullptr");
    if(m_writer_decoded)
    {
      m_writer_decoded.reset(nullptr);
    }
    else error("Writer_decoded is nullptr");
    if(m_event_writer)
    {
        m_event_writer.reset(nullptr);
    }
    else error("m_event_writer  is nullptr");
    return true;
  }
  bool on_configure() override
  {
    generate_folder_name();
    if(m_name.empty())
    {
        error("A filename should be given");
        return false;
    }

    std::error_code ec;
    if(!std::filesystem::create_directories(m_path / m_folder, ec))
    {
        if(ec)
        {
            error("Failed to create directory: {}",ec.message());
            return false;
        }
    }
    std::filesystem::path filename = m_path / m_folder / add_root_extension(m_name);
    std::filesystem::create_directories(m_path / m_folder);
    info("Creating ROOT file: {}", filename);
    m_writer = ROOT::RNTupleWriter::Recreate(std::move(m_model), "raw_data", filename.string());

    std::filesystem::path filename_decoded = m_path / m_folder / add_root_extension(m_name+"_decoded");
    info("Creating ROOT file: {}", filename_decoded);
    m_writer_decoded = ROOT::RNTupleWriter::Recreate(std::move(m_model_decoded), "raw_data_decoded", filename_decoded.string());

    std::filesystem::path filename_hits = m_path / m_folder / add_root_extension(m_name+"_hits");
    info("Creating ROOT file: {}", filename_hits);
    m_event_writer = ROOT::RNTupleWriter::Recreate(std::move(m_model_event), "hits", filename_hits.string());

    Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor ); //ROOT is doing bad stufs
    return true;
  }
  void setTitle(const std::string_view title)
  {
    m_title = title;
  }
  void setName(const std::string_view name)
  {
    m_name = name;
  }
  void setPath(const std::string_view path)
  {
    m_path = path;
    if(!std::filesystem::exists(m_path) || !m_path.is_absolute() || !m_path.filename().empty()) throw yaodaq::Exception("Path must be absolute and must exist"); 
  }
  void onRawData( const std::unique_ptr<yaodaq::RawData> raw ) override
  {
    if(raw->topic()=="MPI::DCT::Singlets::RawData")
    {
      clear();
      DCT::PreProcessedEvent pre_events;
      DCT::Event events;
      std::string_view j(reinterpret_cast<const char*>(raw->payload().data()),raw->payload().size());
      nlohmann::json json = nlohmann::json::parse(j);
      for (const auto& element : json)
      {
        const std::uint32_t word{static_cast<std::uint32_t>(std::stoul(element["word"].get<std::string>(), nullptr, 16))};
        const std::uint32_t bcid{static_cast<std::uint32_t>(std::stoul(element["bcid"].get<std::string>(), nullptr, 16))};
        fill(word,bcid);
        DCT::DecodedRawData raw_data(word,bcid);
        if(raw_data.is_trigger())
        {
            warn("{} bcid: {}, word: {}, rising: {}, falling: {}, raw_bcid: {}, time1: {}, time2: {}, channel: {}",
                 fmt::styled("TRIGGER!",fmt::fg(fmt::color::red) | fmt::emphasis::bold),
                 bcid,
                 word,
                 raw_data.is_raise(),
                 raw_data.is_fall(),
                 raw_data.get_bcid(),
                 raw_data.get_eta1_fine_time(),
                 raw_data.get_eta2_fine_time(),
                 raw_data.get_channel());
            pre_events.triggers.push_back(raw_data);
            events.push_back(raw_data);
        }
        else
        {
            warn("bcid: {}, word: {}, rising: {}, falling: {}, raw_bcid: {}, time1: {}, time2: {}, channel: {}",
            bcid, word, raw_data.is_raise(), raw_data.is_fall(), raw_data.get_bcid(), raw_data.get_eta1_fine_time(), raw_data.get_eta2_fine_time(), raw_data.get_channel());
            pre_events.hits.push_back(raw_data);
            events.push_back(raw_data);
        }     

      }
      if(m_writer) m_writer->Fill();
      if(m_writer_decoded)
      {
        *m_event = pre_events;
        m_writer_decoded->Fill();
      }
      if(m_event_writer)
      {
        *m_real_event = events;
        m_event_writer->Fill();
      }
      // send the events to our fellow analysers ;)
      TString event_json = TBufferJSON::ToJSON(&events);
      send(yaodaq::RawDataBuilder::from_text(event_json.Data(),"MPI::DCT::Singlets::Events"));
      warn("Event {}",event_json.Data());
    }   
    else info("Received {}", raw->topic() );
  }
private:
  void fill(const std::uint32_t word, const std::uint32_t bcid)
  {
    if(m_bcid) m_bcid->push_back(bcid);
    if(m_word) m_word->push_back(word);
  }
  void clear()
  {
    if(m_bcid) m_bcid->clear();
    if(m_word) m_word->clear();
    if(m_event)
    {
        m_event->hits.clear();
        m_event->triggers.clear();
    }
  }
  void generate_folder_name()
  {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    m_folder = fmt::format("{:%Y%m%d}", tm);
  }
  std::string add_root_extension(std::string_view filename)
  {
    std::filesystem::path p(filename);
    if (!p.has_extension())
    {
        p += ".root";   // or p.replace_extension(".root");
        warn("Appending .root to {}", m_name);
    }
    return p.string();
  }
  //TFile m_file;
  std::filesystem::path m_path;
  std::string m_name;
  std::string m_title;
  std::string m_folder;
  // rawdata
  std::unique_ptr<ROOT::RNTupleModel> m_model{nullptr};
  std::unique_ptr<ROOT::RNTupleWriter> m_writer{nullptr};
  std::shared_ptr<std::vector<std::uint32_t>> m_word{nullptr};
  std::shared_ptr<std::vector<std::uint32_t>> m_bcid{nullptr};
  // rawdata_decoded
  std::unique_ptr<ROOT::RNTupleModel> m_model_decoded{nullptr};
  std::unique_ptr<ROOT::RNTupleWriter> m_writer_decoded{nullptr};
  std::shared_ptr<DCT::PreProcessedEvent> m_event{nullptr};
  // event
  std::unique_ptr<ROOT::RNTupleModel> m_model_event{nullptr};
  std::shared_ptr<DCT::Event> m_real_event{nullptr};
  std::unique_ptr<ROOT::RNTupleWriter> m_event_writer{nullptr};





};


int main( int argc, char* argv[] )
try
{
  Term::terminal.setOptions( Term::Option::Raw, Term::Option::Cursor );
  CLI::App app{ "YAODAQ client" };
  argv = app.ensure_utf8( argv );
  std::string host{ "127.0.0.1" };
  app.add_option( "-i,--ip", host, "IP of the server" ) /*->check( CLI::ValidIPV4 )*/;
  int port{ 8888 };
  app.add_option( "-p,--port", port, "Port to listen" )->check( CLI::Range( 0, 65535 ) );
  std::string path{"/data/RPC/YAODAQ/"};
  app.add_option( "--path", path, "Path where to store the file" );
  std::string file_name;
  app.add_option( "--file_name", file_name, "filename folder");
  try
  {
    app.parse( argc, argv );
  }
  catch( const CLI::ParseError& e )
  {
    return app.exit( e );
  }
  yaodaq::Config cfg;
  cfg.setPort( port ).setHost( host );
  FileWriter module( cfg, "DCT" );
  module.setPath(path);
  module.setName(file_name);
  module.link();

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
        else
        {
          nbrCTLC = 3;
          Term::cout << Term::color_fg( Term::Color::Name::Red ) << "Press Ctrl+Q " << std::to_string( nbrCTLC ) << " times to quit" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
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
  Term::cerr << Term::color_fg( Term::Color::Name::Red ) << "error" << Term::color_fg( Term::Color::Name::Default ) << std::endl;
}
