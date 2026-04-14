include(CLI11)
include(cpp-terminal)

add_executable(YAODAQModule YAODAQModule.cpp)
target_link_libraries(YAODAQModule PRIVATE yaodaq::yaodaq CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog)

add_executable(YAODAQController YAODAQController.cpp)
target_link_libraries(YAODAQController PRIVATE yaodaq::yaodaq CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog)

add_executable(YAODAQLogger YAODAQLogger.cpp)
target_link_libraries(YAODAQLogger PRIVATE yaodaq::yaodaq CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog)

add_executable(YAODAQServer YAODAQServer.cpp)
target_link_libraries(YAODAQServer PRIVATE yaodaq::yaodaq CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog)

add_executable(YAODAQBoard YAODAQBoard.cpp)
target_link_libraries(YAODAQBoard PRIVATE yaodaq::yaodaq CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog)
