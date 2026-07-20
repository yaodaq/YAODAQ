#include "yaodaq/transport/Process.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/prctl.h>
#include <poll.h>
#include <stdexcept>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace
{
// environment
extern char** environ;
}  // namespace

yaodaq::ProcessTransport::ProcessTransport( std::string_view name ) : ITransport( name, "Process" )
{
  struct sigaction sa{};
  sa.sa_handler = SIG_IGN;
  sigaction( SIGPIPE, &sa, nullptr );
}

yaodaq::ProcessTransport::~ProcessTransport() { close(); }

bool yaodaq::ProcessTransport::open()
{
  if( m_running ) return false;
  info( "open" );
  m_pid           = -1;
  m_stdin         = -1;
  m_stdout        = -1;
  m_stderr        = -1;
  auto executable = getParameters().get_as<std::string>( "executable" ).value();
  auto args       = getParameters().get_as<yaodaq::Parameters::string_list>( "args" ).value_or( std::vector<std::string>() );
  auto env        = getParameters().get_as<yaodaq::Parameters::string_list>( "env" ).value_or( std::vector<std::string>() );

  int stdin_pipe[2];
  int stdout_pipe[2];
  int stderr_pipe[2];
  if( pipe2( stdin_pipe, O_CLOEXEC ) != 0 ) return false;
  if( pipe2( stdout_pipe, O_CLOEXEC ) != 0 )
  {
    ::close( stdin_pipe[0] );
    ::close( stdin_pipe[1] );
    return false;
  }
  if( pipe2( stderr_pipe, O_CLOEXEC ) != 0 )
  {
    ::close( stdin_pipe[0] );
    ::close( stdin_pipe[1] );
    ::close( stdout_pipe[0] );
    ::close( stdout_pipe[1] );
    return false;
  }
  // argv
  std::vector<std::string> argv_storage;
  argv_storage.push_back( executable );
  for( auto& a: args ) argv_storage.push_back( a );
  std::vector<char*> argv;
  for( auto& a: argv_storage ) argv.push_back( a.data() );
  argv.push_back( nullptr );
  // env
  std::vector<std::string> env_storage;
  for( char** e = ::environ; *e; ++e ) env_storage.emplace_back( *e );
  for( auto& variable: env )
  {
    auto pos = variable.find( '=' );
    if( pos != std::string::npos )
    {
      auto key = variable.substr( 0, pos );
      env_storage.erase( std::remove_if( env_storage.begin(), env_storage.end(), [&]( const std::string& value ) { return value.rfind( key + "=", 0 ) == 0; } ), env_storage.end() );
    }
    env_storage.push_back( variable );
  }
  std::vector<char*> envp;
  for( auto& e: env_storage ) envp.push_back( e.data() );
  envp.push_back( nullptr );
  pid_t pid = fork();
  if( pid < 0 )
  {
    ::close( stdin_pipe[0] );
    ::close( stdin_pipe[1] );

    ::close( stdout_pipe[0] );
    ::close( stdout_pipe[1] );

    ::close( stderr_pipe[0] );
    ::close( stderr_pipe[1] );

    return false;
  }
  // Child
  if( pid == 0 )
  {
    signal( SIGPIPE, SIG_DFL );
    // Create process group. PGID == PID
    if( setpgid( 0, 0 ) != 0 ) _exit( 1 );
    // Parent death protection.
    pid_t parent = getppid();
    if( prctl( PR_SET_PDEATHSIG, SIGTERM ) != 0 ) _exit( 1 );
    // Race: parent died before prctl.
    if( getppid() != parent ) _exit( 1 );
    if( dup2( stdin_pipe[0], STDIN_FILENO ) < 0 ) _exit( 1 );
    if( dup2( stdout_pipe[1], STDOUT_FILENO ) < 0 ) _exit( 1 );
    if( dup2( stderr_pipe[1], STDERR_FILENO ) < 0 ) _exit( 1 );
    // close inherited descriptors
    ::close( stdin_pipe[0] );
    ::close( stdin_pipe[1] );

    ::close( stdout_pipe[0] );
    ::close( stdout_pipe[1] );

    ::close( stderr_pipe[0] );
    ::close( stderr_pipe[1] );
    int ret = execve( executable.c_str(), argv.data(), envp.data() );
    _exit( 127 );
  }
  // Parent;
  m_pid = pid;
  // Make sure PGID exists.
  setpgid( pid, pid );

  // parent does not read stdin
  ::close( stdin_pipe[0] );

  // parent does not write stdout/stderr
  ::close( stdout_pipe[1] );
  ::close( stderr_pipe[1] );

  m_stdin  = stdin_pipe[1];
  m_stdout = stdout_pipe[0];
  m_stderr = stderr_pipe[0];
  //int flags = fcntl(m_stdout,F_GETFL,0);
  //fcntl(m_stdout,F_SETFL,flags | O_NONBLOCK);
  m_exitSignal.reset();
  m_exitCode.reset();
  m_running = true;
  return true;
}

bool yaodaq::ProcessTransport::close()
{
  if( !m_running && m_pid <= 0 ) return true;

  if( m_stdin >= 0 )
  {
    ::close( m_stdin );
    m_stdin = -1;
  }

  if( m_stdout >= 0 )
  {
    ::close( m_stdout );
    m_stdout = -1;
  }

  if( m_stderr >= 0 )
  {
    ::close( m_stderr );
    m_stderr = -1;
  }

  if( m_pid > 0 && m_running )
  {
    // Graceful shutdown.
    ::kill( -m_pid, SIGTERM );
    for( int i = 0; i < 100; i++ )
    {
      if( update() ) break;
      std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    }
    // hard kill
    if( m_running )
    {
      ::kill( -m_pid, SIGKILL );
      int status{ 0 };
      waitpid( m_pid, &status, 0 );
      if( WIFSIGNALED( status ) ) m_exitSignal = WTERMSIG( status );
      if( WIFEXITED( status ) ) m_exitCode = WEXITSTATUS( status );
      m_running = false;
    }
  }
  m_pid    = -1;
  m_stdin  = -1;
  m_stdout = -1;
  m_stderr = -1;
  return true;
}

void yaodaq::ProcessTransport::write( std::span<const std::byte> data )
{
  if( !m_running || m_stdin < 0 ) return;
  std::string rr( reinterpret_cast<const char*>( data.data() ), data.size() );
  const char* ptr       = reinterpret_cast<const char*>( data.data() );
  std::size_t remaining = data.size();
  while( remaining )
  {
    auto n = ::write( m_stdin, ptr, remaining );
    if( n < 0 )
    {
      if( errno == EINTR ) continue;
      throw std::runtime_error( std::strerror( errno ) );
    }
    ptr += n;
    remaining -= n;
  }
}

std::vector<yaodaq::TransportPacket> yaodaq::ProcessTransport::read()
{
  if( !m_running ) return {};
  std::vector<yaodaq::TransportPacket> ret;

  struct pollfd fds[2];
  fds[0].fd      = m_stdout;
  fds[0].events  = POLLIN;
  fds[0].revents = 0;

  fds[1].fd      = m_stderr;
  fds[1].events  = POLLIN;
  fds[1].revents = 0;

  int result{ 0 };
  while( true )
  {
    result = ::poll( fds, 2, 100 );  // 100 ms timeout
    if( result > 0 ) break;
    if( result == 0 )
    {
      // Timeout: see whether the child exited.
      if( !m_running ) return ret;
      continue;
    }
    if( errno == EINTR ) continue;
    return ret;
  }
  std::byte buffer[4096];
  auto      readFd = [&]( const int fd, const short revents, const std::string_view channel )
  {
    if( !( revents & ( POLLIN | POLLHUP | POLLERR ) ) ) return;
    while( true )
    {
      ssize_t n;
      do { n = ::read( fd, buffer, sizeof( buffer ) ); } while( n < 0 && errno == EINTR );
      if( n > 0 )
      {
        ret.emplace_back( ByteBufferView( buffer, static_cast<size_t>( n ) ), channel );
        // Stop here. The next call can read more data.
        break;
      }
      if( n == 0 )
      {
        // Pipe closed
        update();
        break;
      }
      // Error
      if( errno == EAGAIN || errno == EWOULDBLOCK ) break;
      break;
    }
  };
  readFd( m_stdout, fds[0].revents, "stdout" );
  readFd( m_stderr, fds[1].revents, "stderr" );
  return ret;
}

bool yaodaq::ProcessTransport::update()
{
  if( !m_running ) return true;

  int   status{};
  pid_t result = waitpid( m_pid, &status, WNOHANG );

  if( result <= 0 ) return false;

  m_running = false;

  if( WIFSIGNALED( status ) )
  {
    m_exitSignal = WTERMSIG( status );
    m_exitCode.reset();
  }
  else if( WIFEXITED( status ) )
  {
    m_exitCode = WEXITSTATUS( status );
    m_exitSignal.reset();
  }

  return true;
}

bool yaodaq::ProcessTransport::sendSignal( int signal )
{
  if( !m_running || m_pid <= 0 ) return false;
  return ::kill( -m_pid, signal ) == 0;
}

bool yaodaq::ProcessTransport::terminate() { return sendSignal( SIGTERM ); }

bool yaodaq::ProcessTransport::kill() { return sendSignal( SIGKILL ); }

bool yaodaq::ProcessTransport::verifyParameters()
{
  auto executable = getParameters().get_as_f<std::string>( "executable" );
  return executable.has_value() && !executable.raw()->empty();
}
