include(doctest)

add_executable(message.test message.test.cpp)
target_link_libraries(message.test PRIVATE yaodaq::yaodaq doctest::doctest_with_main)

add_executable(clock.test clock.test.cpp)
target_link_libraries(clock.test PRIVATE yaodaq::yaodaq doctest::doctest_with_main)
