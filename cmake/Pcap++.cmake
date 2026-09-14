include_guard(GLOBAL)

include(CPM)
cpm(SYSTEM SHALLOW PROGRESS EXCLUDE_FROM_ALL)

if(NOT DEFINED PCAP++_REPOSITORY)
  set(PCAP++_REPOSITORY "https://github.com/seladb/PcapPlusPlus.git")
endif()

if(NOT DEFINED PCAP++_TAG)
  set(PCAP++_TAG "v26.07")
endif()

CPMAddPackage(NAME Pcap++
              GIT_SHALLOW TRUE
              GIT_REPOSITORY "${PCAP++_REPOSITORY}"
              GIT_TAG "${PCAP++_TAG}"
              OPTIONS "PCAPPP_BUILD_TESTS  OFF")
target_compile_options(Packet++ PRIVATE -Wno-maybe-uninitialized)
