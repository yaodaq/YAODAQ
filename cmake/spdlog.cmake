include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

include(fmt)

if(NOT DEFINED SPDLOG_REPOSITORY)
  set(SPDLOG_REPOSITORY "https://github.com/gabime/spdlog.git")
endif()

if(NOT DEFINED SPDLOG_TAG)
  set(SPDLOG_TAG "v1.17.0")
endif()

CPMAddPackage(NAME spdlog
              GIT_REPOSITORY ${SPDLOG_REPOSITORY}
              GIT_TAG ${SPDLOG_TAG}
              OPTIONS "SPDLOG_BUILD_ALL OFF"
                      "SPDLOG_ENABLE_PCH ON"
                      "SPDLOG_BUILD_EXAMPLE OFF"
                      "SPDLOG_BUILD_EXAMPLE_HO OFF"
                      "SPDLOG_BUILD_TESTS OFF"
                      "SPDLOG_BUILD_TESTS_HO OFF"
                      "SPDLOG_SANITIZE_ADDRESS OFF"
                      "SPDLOG_INSTALL OFF"
                      "SPDLOG_FMT_EXTERNAL ON"
                      "SPDLOG_FMT_EXTERNAL_HO OFF"
                      "SPDLOG_NO_EXCEPTIONS OFF")
