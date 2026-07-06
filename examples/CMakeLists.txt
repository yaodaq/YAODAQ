
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

find_package(ROOT)

add_executable(DCT RPC_DCT.cpp)
target_link_libraries(DCT PRIVATE yaodaq::yaodaq CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog csv)

add_library(Data SHARED)
target_compile_features(Data PUBLIC cxx_std_20)
root_generate_dictionary(RawData_Dict RawData.hpp Data.hpp MODULE Data LINKDEF LinkDef.h)

add_executable(FileWriter DCT_FileWriter.cpp)
target_link_libraries(FileWriter PRIVATE Data yaodaq::yaodaq CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog ROOT::Core ROOT::RIO ROOT::ROOTNTuple)
# Attach dictionaries to the executable. First, tell it where to look for headers required by the dictionaries:
target_include_directories(FileWriter PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/example>)

add_executable(Analysor Analysor.cpp)
target_link_libraries(Analysor PRIVATE yaodaq::yaodaq Data CLI11::CLI11 cpp-terminal::cpp-terminal spdlog::spdlog ROOT::Core ROOT::RIO)
