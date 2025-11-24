/**
\copyright Copyright 2022 flagarde
*/

#include "yaodaq/Looper.hpp"

#include <chrono>
#include <thread>

namespace yaodaq
{

int Looper::m_instance{ 0 };

Interrupt Looper::m_Interrupt{ Interrupt{} };

int Looper::getInstance() { return m_instance; }

void Looper::supressInstance()
{
  if( m_hasBeenSupressed == false )
  {
    m_hasBeenSupressed = true;
    m_instance--;
  }
}

Looper::Looper()
{
  if( m_hasBeenAdded == false )
  {
    m_hasBeenAdded = true;
    ++m_instance;
  }
}

Signal Looper::loop()
{
  static Signal signal{ yaodaq::Signal::NO };
  if( m_instance == 0 )
  {
    do
    {
      signal = m_Interrupt.getSignal();
      std::this_thread::sleep_for( std::chrono::microseconds( 1 ) );
    } while( signal == yaodaq::Signal::NO );
  }
  return signal;
}

Signal Looper::getSignal() { return m_Interrupt.getSignal(); }

Looper::~Looper()
{
  if( m_hasBeenAdded == true && m_hasBeenSupressed == false )
  {
    m_hasBeenSupressed = true;
    --m_instance;
  }
}

}  // namespace yaodaq
