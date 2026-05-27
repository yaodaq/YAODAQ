include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

if(NOT DEFINED SOCI_REPOSITORY)
  set(SOCI_REPOSITORY "https://github.com/SOCI/soci.git")
endif()

if(NOT DEFINED SOCI_TAG)
  set(SOCI_TAG "master")
endif()

CPMAddPackage(NAME soci
               GIT_REPOSITORY "${SOCI_REPOSITORY}"
               GIT_TAG "${SOCI_TAG}"
               GIT_SUBMODULES ""
               GIT_SHALLOW TRUE
              OPTIONS "SOCI_TESTS FALSE"
                      "SOCI_INSTALL FALSE"
                      "WITH_BOOST FALSE")

