include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

if(NOT DEFINED CLI11_REPOSITORY)
  set(CLI11_REPOSITORY "https://github.com/CLIUtils/CLI11.git")
endif()

if(NOT DEFINED CLI11_TAG)
  set(CLI11_TAG "v2.3.1")
endif()

CPMAddPackage(NAME CLI11
               GIT_REPOSITORY "${CLI11_REPOSITORY}"
               GIT_TAG "${CLI11_TAG}"
               GIT_SUBMODULES ""
              OPTIONS "CLI11_WARNINGS_AS_ERRORS FALSE"
                      "CLI11_SINGLE_FILE FALSE"
                      "CLI11_SANITIZERS FALSE"
                      "CLI11_BUILD_DOCS FALSE"
                      "CLI11_BUILD_TESTS FALSE"
                      "CLI11_BUILD_EXAMPLES FALSE"
                      "CLI11_BUILD_EXAMPLES_JSON FALSE"
                      "CLI11_SINGLE_FILE_TESTS FALSE"
                      "CLI11_INSTALL OFF"
                      "CLI11_FORCE_LIBCXX OFF"
                      "CLI11_CUDA_TESTS OFF"
                      "CLI11_CLANG_TIDY OFF")
