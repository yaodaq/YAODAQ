add_library(yaodaq_websocket_transport SHARED WebSocket.cpp)
target_link_libraries(yaodaq_websocket_transport PUBLIC yaodaq::yaodaq)
target_include_directories(yaodaq_websocket_transport PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/extra> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
add_library(yaodaq::transport::websocket ALIAS yaodaq_websocket_transport)
