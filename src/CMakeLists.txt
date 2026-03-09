include(IXWebSocket)
include(spdlog)
include(MagicEnum)
include(JsonRpcCxx)

add_library(yaodaq SHARED
 Component.cpp
 Client.cpp
 Exception.cpp
 Server.cpp
 JsonRPCHandler.cpp
 Identifier.cpp
 WebsocketCloseConstants.cpp
)
set_target_properties(yaodaq PROPERTIES
                             PUBLIC_HEADER "${PROJECT_SOURCE_DIR}/include/yaodaq/Exception.hpp"
                             CXX_EXTENSIONS FALSE
                             CXX_STANDARD 17
                             CXX_STANDARD_REQUIRED TRUE
                             POSITION_INDEPENDENT_CODE TRUE)
target_link_libraries(yaodaq PUBLIC ixwebsocket::ixwebsocket json-rpc-cxx::json-rpc-cxx PRIVATE magic_enum::magic_enum fmt::fmt spdlog::spdlog)
target_include_directories(yaodaq PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(yaodaq PUBLIC cxx_std_17)
add_library(yaodaq::yaodaq ALIAS yaodaq)

include(GNUInstallDirs)
install(TARGETS yaodaq
        EXPORT YAODAQTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT YAODAQTargets
        FILE YAODAQTargets.cmake
        NAMESPACE yaodaq::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YAODAQ
)

include(CMakePackageConfigHelpers)
configure_package_config_file("${PROJECT_SOURCE_DIR}/cmake/YAODAQConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/cmake/YAODAQConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YAODAQ
)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/cmake/YAODAQConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY AnyNewerVersion
)
install(FILES
          "${PROJECT_BINARY_DIR}/cmake/YAODAQConfig.cmake"
          "${PROJECT_BINARY_DIR}/cmake/YAODAQConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YAODAQ
)
