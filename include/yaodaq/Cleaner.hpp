#pragma once
#include "yaodaq/Export.hpp"

#include <algorithm>
#include <iostream>
#include <mutex>
#include <vector>

namespace yaodaq
{

class Client;
class Server;

class Cleaner
{
public:
  YAODAQ_API static Cleaner& instance()
  {
    static Cleaner r;
    return r;
  }
  YAODAQ_API static bool add( Client* c );
  YAODAQ_API static bool add( Server* s );
  YAODAQ_API static void remove( Client* c );
  YAODAQ_API static void remove( Server* c );
  YAODAQ_API static void clean();

private:
  YAODAQ_API explicit Cleaner()
  {
    std::cout << "Creating cleaning" << std::endl;
    std::atexit( []() { Cleaner::instance().at_exit_cleanup(); } );
  }
  YAODAQ_API void                    at_exit_cleanup() { Cleaner::clean(); }
  inline static std::mutex           m_mutex;
  inline static std::vector<Client*> m_clients;
  inline static std::vector<Server*> m_server;
};

}  // namespace yaodaq
