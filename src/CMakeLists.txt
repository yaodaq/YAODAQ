set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

include(IXWebSocket)
include(spdlog)
include(MagicEnum)
include(cxx-jsonrpc)
include(cpp-terminal)

configure_file("${PROJECT_SOURCE_DIR}/include/yaodaq/Version.hpp.in" "${PROJECT_BINARY_DIR}/include/yaodaq/Version.hpp" @ONLY)

add_library(yaodaq SHARED
 Component.cpp
 Client.cpp
 Exception.cpp
 Server.cpp
 Controller.cpp
 Log.cpp
 JsonRPCAsker.cpp
 MetaInfos.cpp
 Identifier.cpp
 Response.cpp
 WebsocketCloseConstants.cpp
 State.cpp
)

list(APPEND YAODAQ_PUBLIC_HEADERS
"${PROJECT_BINARY_DIR}/include/yaodaq/Version.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/Client.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/Module.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/Board.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/Component.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/ConnectionState.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/Exception.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/Identifier.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/JsonRPCHandler.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/Response.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/Server.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/ThreadPool.hpp"
"${PROJECT_SOURCE_DIR}/include/yaodaq/WebsocketCloseConstants.hpp"
)
set_target_properties(yaodaq PROPERTIES
                             PUBLIC_HEADER "${YAODAQ_PUBLIC_HEADERS}"
                             CXX_EXTENSIONS FALSE
                             CXX_STANDARD 17
                             CXX_STANDARD_REQUIRED TRUE
                             CXX_VISIBILITY_PRESET "hidden"
                             VISIBILITY_INLINES_HIDDEN TRUE
                             POSITION_INDEPENDENT_CODE TRUE)
target_link_libraries(yaodaq PUBLIC ixwebsocket::ixwebsocket cxx-jsonrpc::cxx-jsonrpc PRIVATE magic_enum::magic_enum spdlog::spdlog fmt::fmt cpp-terminal::cpp-terminal nlohmann_json::nlohmann_json)
target_compile_definitions(yaodaq PRIVATE $<$<STREQUAL:$<TARGET_PROPERTY:yaodaq,TYPE>,SHARED_LIBRARY>:YAODAQ_BUILD_DLL=1> $<$<STREQUAL:$<TARGET_PROPERTY:yaodaq,TYPE>,SHARED_LIBRARY>:CXX_JSONRPC_BUILD_DLL=1>)
target_include_directories(yaodaq PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(yaodaq PUBLIC cxx_std_17)
add_library(yaodaq::yaodaq ALIAS yaodaq)

include(GNUInstallDirs)
install(TARGETS yaodaq spdlog fmt
        EXPORT YAODAQTargets
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/yaodaq")
install(EXPORT YAODAQTargets
        FILE YAODAQTargets.cmake
        NAMESPACE yaodaq::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YAODAQ)

include(CMakePackageConfigHelpers)
configure_package_config_file("${PROJECT_SOURCE_DIR}/cmake/YAODAQConfig.cmake.in" "${PROJECT_BINARY_DIR}/cmake/YAODAQConfig.cmake" INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YAODAQ)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/cmake/YAODAQConfigVersion.cmake" VERSION "${PROJECT_VERSION}" COMPATIBILITY AnyNewerVersion)
install(FILES "${PROJECT_BINARY_DIR}/cmake/YAODAQConfig.cmake" "${PROJECT_BINARY_DIR}/cmake/YAODAQConfigVersion.cmake" DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/YAODAQ")
