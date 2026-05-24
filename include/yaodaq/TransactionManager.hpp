#pragma once

#include "yaodaq/Export.hpp"
#include "yaodaq/Message.hpp"
#include "yaodaq/Exception.hpp"
#include <future>
#include <mutex>
#include <unordered_map>

class TransactionManager
{
public:
  std::future<yaodaq::Message> create(std::string uuid)
  {
    std::promise<yaodaq::Message> promise;
    auto future = promise.get_future();
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      auto [it, inserted] = m_pending.emplace(std::move(uuid),std::move(promise));
      if(!inserted) throw yaodaq::Exception("Duplicate transaction UUID");
    }
    return future;
  }

  bool resolve(yaodaq::Message msg)
  {
    std::promise<yaodaq::Message> promise;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      auto it = m_pending.find(msg.uuid());
      if (it == m_pending.end()) return false;
      promise = std::move(it->second);
      m_pending.erase(it);
    }
    promise.set_value(std::move(msg));
    return true;
  }

  void shutdown()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.clear();
  }

private:
  std::unordered_map<std::string,std::promise<yaodaq::Message>> m_pending;
  std::mutex m_mutex;
};
