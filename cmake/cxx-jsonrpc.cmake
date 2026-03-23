include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

include(Nlohmann)

if(NOT DEFINED CXX_JSONRPC_REPOSITORY)
  set(CXX_JSONRPC_REPOSITORY "https://github.com/cxx-libs/cxx-json-rpc.git")
endif()

if(NOT DEFINED CXX_JSONRPC_TAG)
  set(CXX_JSONRPC_TAG "main")
endif()

CPMAddPackage(NAME cxx-jsonrpc
    GIT_REPOSITORY ${CXX_JSONRPC_REPOSITORY}
    GIT_TAG ${CXX_JSONRPC_TAG})
