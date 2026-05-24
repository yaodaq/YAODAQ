#pragma once

#include "Message.hpp"

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

class Dispatcher
{
public:

  using Handler =std::function<void(const yaodaq::Message&)>;

  void subscribe(const yaodaq::Message::Type type, Handler handler)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handlers[type].push_back(std::move(handler));
  }

  void subscribeToAll(Handler handler)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_all.push_back(std::move(handler));
  }

  void dispatch(const yaodaq::Message& msg)
  {
    std::vector<Handler> handlers;
    std::vector<Handler> all;

    {
        std::lock_guard<std::mutex>
            lock(m_mutex);

        auto it = m_handlers.find(msg.type());

        if(it != m_handlers.end())
        {
            handlers = it->second;
        }

        all = m_all;
    }

    for(auto& handler : handlers)
    {
        handler(msg);
    }

    for(auto& handler : all)
    {
        handler(msg);
    }
  }

private:
  std::unordered_map<yaodaq::Message::Type,std::vector<Handler>> m_handlers;
  std::vector<Handler> m_all;
  std::mutex m_mutex;
};
