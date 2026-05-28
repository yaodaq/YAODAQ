include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

if(NOT DEFINED IXWEBSOCKET_REPOSITORY)
  set(IXWEBSOCKET_REPOSITORY "https://github.com/cxx-libs/IXWebSocket.git")
endif()

if(NOT DEFINED IXWEBSOCKET_TAG)
  set(IXWEBSOCKET_TAG "accept_n_only_headers")
endif()

include(Zlib-ng)
CPMAddPackage(NAME IXWebSocket
              GIT_SHALLOW TRUE
              GIT_REPOSITORY "${IXWEBSOCKET_REPOSITORY}"
              GIT_TAG "${IXWEBSOCKET_TAG}"
              OPTIONS "BUILD_DEMO FALSE" "USE_TLS TRUE" "USE_ZLIB TRUE" "USE_WS FALSE")
