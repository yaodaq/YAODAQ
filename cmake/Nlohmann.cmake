include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

if(NOT DEFINED NLOHMANN_REPOSITORY)
  set(NLOHMANN_REPOSITORY "https://github.com/nlohmann/json.git")
endif()

if(NOT DEFINED NLOHMANN_TAG)
  set(NLOHMANN_TAG "v3.12.0")
endif()

CPMAddPackage(NAME nlohmann
        GIT_REPOSITORY ${NLOHMANN_REPOSITORY}
        GIT_TAG ${NLOHMANN_TAG}
        OPTIONS "JSON_BuildTests FALSE"
                "JSON_CI FALSE"
                "JSON_Diagnostics TRUE"
                "JSON_Diagnostic_Positions TRUE"
                "JSON_GlobalUDLs TRUE"
                "JSON_ImplicitConversions TRUE"
                "JSON_DisableEnumSerialization FALSE"
                "JSON_LegacyDiscardedValueComparison FALSE"
                "JSON_Install TRUE"
                "JSON_MultipleHeaders TRUE"
                "JSON_SystemInclude FALSE")
