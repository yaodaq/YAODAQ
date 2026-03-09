include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

include(Nlohmann)

if(NOT DEFINED JSON_RPC_CXX_REPOSITORY)
  set(JSON_RPC_CXX_REPOSITORY "https://github.com/flagarde/json-rpc-cxx.git")
endif()

if(NOT DEFINED JSON_RPC_CXX_TAG)
  set(JSON_RPC_CXX_TAG "master")
endif()

CPMAddPackage(NAME json-rpc-cxx
    GIT_REPOSITORY ${JSON_RPC_CXX_REPOSITORY}
    GIT_TAG ${JSON_RPC_CXX_TAG}
    OPTIONS "COMPILE_TESTS FALSE"
            "COMPILE_EXAMPLES FALSE"
            "CODE_COVERAGE FALSE")
