#pragma once
#include "yaodaq/ITransport.hpp"
#include <span>
#include <optional>
#include <vector>
#include <string>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>

namespace yaodaq{

class ProcessTransport : public yaodaq::ITransport
{
public:
  ProcessTransport(std::string_view name);
  ~ProcessTransport() override;
  bool open() override;
  bool close() override;
  void write(std::span<const std::byte> data) override;

  std::vector<TransportPacket> read() override;

  bool verifyParameters() override;
  /**
  * Poll child state.
  *
  * Returns true if child exited.
  */
  bool update();
  std::optional<int> exitSignal() const noexcept
  {
    return m_exitSignal;
  }
  std::optional<int> exitCode() const noexcept
  {
    return m_exitCode;
  }

  /**
  * Ask process tree to exit gracefully.
  */
  bool terminate();
  /**
  * Force kill process tree.
  */
  bool kill();
  /**
  * Send custom signal to process tree.
  */
  bool sendSignal(int signal);
  bool isRunning() const noexcept
  {
    return m_running;
  }
private:
  pid_t m_pid{-1};
  int m_stdin{-1};   // parent writes here
  int m_stdout{-1};  // parent reads here
  int m_stderr{-1};  // parent reads here
  int m_killSignal{SIGTERM};
  bool m_running{false};
  std::optional<int> m_exitSignal;
  std::optional<int> m_exitCode;
};

}
