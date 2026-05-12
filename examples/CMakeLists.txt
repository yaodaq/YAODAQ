
# Fetch csv-parser and make it available to my-executable
set(ENABLE_CODE_COVERAGE OFF)
set(CSV_BUILD_PROGRAMS OFF)
set(CSV_BUILD_TESTS OFF)
FetchContent_Declare(
  csv
  GIT_REPOSITORY https://github.com/vincentlaucsb/csv-parser.git
  GIT_TAG 3.5.1
)
FetchContent_MakeAvailable(csv)

add_executable(TDC RPC_TDC.cpp)
target_link_libraries(TDC PRIVATE yaodaq::yaodaq CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog csv)
