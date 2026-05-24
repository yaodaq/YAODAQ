#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template<typename T> class ThreadSafeQueue
{
public:
  ThreadSafeQueue() noexcept = default;
  void push( T value )
  {
    {
      std::lock_guard<std::mutex> lock( mutex_ );
      if( shutdown_ ) return;
      queue_.push( std::move( value ) );
    }
    cv_.notify_one();
  }

  std::optional<T> pop()
  {
    std::unique_lock<std::mutex> lock( mutex_ );

    cv_.wait( lock, [&] { return shutdown_ || !queue_.empty(); } );

    if( shutdown_ && queue_.empty() ) { return std::nullopt; }

    T value = std::move( queue_.front() );

    queue_.pop();

    return value;
  }

  void shutdown()
  {
    {
      std::lock_guard<std::mutex> lock( mutex_ );

      shutdown_ = true;

      std::queue<T> empty;

      std::swap( queue_, empty );
    }

    cv_.notify_all();
  }

private:
  std::queue<T> queue_;

  std::mutex mutex_;

  std::condition_variable cv_;

  bool shutdown_{ false };
};
