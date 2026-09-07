include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

if(NOT DEFINED CXX_SERIAL_REPOSITORY)
  set(CXX_SERIAL_REPOSITORY "https://github.com/cxx-libs/cxx-serial.git")
endif()

if(NOT DEFINED CXX_SERIAL_TAG)
  set(CXX_SERIAL_TAG "main")
endif()

CPMAddPackage(NAME cxx-serial
    GIT_SHALLOW TRUE
    GIT_REPOSITORY ${CXX_SERIAL_REPOSITORY}
    GIT_TAG ${CXX_SERIAL_TAG})
