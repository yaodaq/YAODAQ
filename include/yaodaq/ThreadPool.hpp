#pragma once
#include "yaodaq/Exception.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace yaodaq
{

class ThreadPool
{
public:
  explicit ThreadPool( const std::size_t threads = std::max( 1u, std::thread::hardware_concurrency() ) )
  {
    m_workers.reserve( threads );
    for( std::size_t i = 0; i < threads; ++i )
    {
      m_workers.emplace_back(
        [this]
        {
          while( true )
          {
            std::function<void()> task;
            {
              std::unique_lock<std::mutex> lock( m_mutex );
              m_cv.wait( lock, [this] { return m_stop || !m_tasks.empty(); } );
              if( m_stop && m_tasks.empty() ) return;
              task = std::move( m_tasks.front() );
              m_tasks.pop();
            }
            // execute outside lock
            task();
          }
        } );
    }
  }

  ~ThreadPool()
  {
    {
      std::unique_lock<std::mutex> lock( m_mutex );
      m_stop = true;
    }
    m_cv.notify_all();
    for( auto& worker: m_workers )
    {
      if( worker.joinable() ) worker.join();
    }
  }

  // =========================================================
  // enqueue with return value + exception propagation
  // =========================================================
  template<class F, class... Args> auto enqueue( F&& f, Args&&... args ) -> std::future<std::invoke_result_t<F, Args...>>
  {
    using return_type               = std::invoke_result_t<F, Args...>;
    auto                     task   = std::make_shared<std::packaged_task<return_type()>>( std::bind( std::forward<F>( f ), std::forward<Args>( args )... ) );
    std::future<return_type> result = task->get_future();
    {
      std::unique_lock<std::mutex> lock( m_mutex );
      if( m_stop ) throw yaodaq::Exception( "enqueue on stopped ThreadPool" );
      m_tasks.emplace( [task]() { ( *task )(); } );
    }
    m_cv.notify_one();
    return result;
  }

private:
  std::vector<std::thread>          m_workers;
  std::queue<std::function<void()>> m_tasks;
  std::mutex                        m_mutex;
  std::condition_variable           m_cv;
  bool                              m_stop{ false };
};

}  // namespace yaodaq
