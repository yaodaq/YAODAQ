add_executable(process.test process.test.cpp)
target_link_libraries(process.test PRIVATE csv yaodaq::yaodaq yaodaq::transport::process doctest::doctest_with_main CLI11::CLI11 cpp-terminal::cpp-terminal)
add_test(NAME process.test COMMAND process.test)
set_tests_properties(process.test PROPERTIES WORKING_DIRECTORY $<TARGET_FILE_DIR:process.test>)
