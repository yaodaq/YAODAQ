cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GetCMakeMM)
cmmm(VERSION 1.1)

cmmm_modules_list(URL "gh:cmake-tools/cmcm")

project(YAODAQ VERSION 0.0.0.1 DESCRIPTION "Yet An Other DAQ" HOMEPAGE_URL "https://github.com/yaodaq/YAODAQ" LANGUAGES CXX)

get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT GENERATOR_IS_MULTI_CONFIG)
  if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
  endif()
  message(STATUS "Compiling type : ${CMAKE_BUILD_TYPE}")
else()
  message("Single-configuration generator")
endif()

option(YAODAQ_TESTS "Compile the tests" OFF)

# File with all dependencies
include("${CMAKE_CURRENT_SOURCE_DIR}/dependencies.cmake")

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

add_subdirectory(src)
add_subdirectory(web)

add_subdirectory(apps)

if(YAODAQ_TESTS)
  #include(CTest)
endif()
