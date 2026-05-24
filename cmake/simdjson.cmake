include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

if(NOT DEFINED SIMDJSON_REPOSITORY)
  set(SIMDJSON_REPOSITORY "https://github.com/simdjson/simdjson.git")
endif()

if(NOT DEFINED SIMDJSON_TAG)
  set(SIMDJSON_TAG "v4.6.4")
endif()

CPMAddPackage(NAME simdjson
        GIT_REPOSITORY ${SIMDJSON_REPOSITORY}
        GIT_TAG ${SIMDJSON_TAG}
        GIT_SHALLOW TRUE)
