#pragma once
#include "yaodaq/Export.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template<typename T> class ThreadSafeQueue
{
public:
  YAODAQ_API void push( T value )
  {
    {
      std::lock_guard<std::mutex> lock( m_mutex );
      if( m_shutdown ) return;
      m_queue.push( std::move( value ) );
    }
    m_cv.notify_one();
  }

  YAODAQ_API std::optional<T> pop()
  {
    std::unique_lock<std::mutex> lock( m_mutex );
    m_cv.wait( lock, [&] { return m_shutdown || !m_queue.empty(); } );
    if( m_queue.empty() ) return std::nullopt;
    T value = std::move( m_queue.front() );
    m_queue.pop();
    return value;
  }

  YAODAQ_API void shutdown()
  {
    {
      std::lock_guard<std::mutex> lock( m_mutex );
      m_shutdown = true;
      std::queue<T>().swap( m_queue );
    }
    m_cv.notify_all();
  }

private:
  std::queue<T>           m_queue;
  std::mutex              m_mutex;
  std::condition_variable m_cv;
  bool                    m_shutdown{ false };
};
