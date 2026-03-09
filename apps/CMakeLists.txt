include(CLI11)
include(cpp-terminal)

add_executable(YAODAQClient YAODAQClient.cpp)
target_link_libraries(YAODAQClient PRIVATE yaodaq::yaodaq CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog)

add_executable(YAODAQServer YAODAQServer.cpp)
target_link_libraries(YAODAQServer PRIVATE yaodaq::yaodaq CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog)
