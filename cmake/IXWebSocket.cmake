include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

if(NOT DEFINED IXWEBSOCKET_REPOSITORY)
  set(IXWEBSOCKET_REPOSITORY "https://github.com/machinezone/IXWebSocket")
endif()

if(NOT DEFINED IXWEBSOCKET_TAG)
  set(IXWEBSOCKET_TAG "v11.4.6")
endif()

include(Zlib-ng)
CPMAddPackage(NAME IXWebSocket
              GIT_REPOSITORY "${IXWEBSOCKET_REPOSITORY}"
              GIT_TAG "${IXWEBSOCKET_TAG}"
              OPTIONS "BUILD_DEMO 0" "USE_TLS 0" "USE_ZLIB 1" "USE_WS 0")
