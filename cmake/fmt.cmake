include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

if(NOT DEFINED FMT_REPOSITORY)
  set(FMT_REPOSITORY "https://github.com/fmtlib/fmt")
endif()

if(NOT DEFINED FMT_TAG)
  set(FMT_TAG "12.1.0")
endif()

CPMAddPackage(NAME fmt
               GIT_REPOSITORY ${FMT_REPOSITORY}
               GIT_TAG ${FMT_TAG}
               FETCHCONTENT_UPDATES_DISCONNECTED ${IS_OFFLINE}
               OPTIONS "FMT_PEDANTIC OFF"
                       "FMT_WERROR OFF"
                       "FMT_DOC OFF"
                       "FMT_INSTALL OFF"
                       "FMT_FUZZ OFF"
                       "FMT_CUDA_TEST OFF"
                       "FMT_OS ON"
                       "FMT_SYSTEM_HEADERS ON"
                       "FMT_UNICODE ON")
