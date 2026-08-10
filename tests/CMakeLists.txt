# Aurora Player - unit tests (308 assertions, no external test framework).
add_executable(aurora-tests test_core.cpp)

target_link_libraries(aurora-tests PRIVATE Aurora::Core aurora_flags)

add_test(NAME aurora-core COMMAND aurora-tests)
set_tests_properties(aurora-core PROPERTIES TIMEOUT 180)
