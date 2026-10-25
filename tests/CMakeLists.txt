add_executable(undertow_tests
  check_main.cpp
  test_conformance.cpp
  test_midi_file.cpp
  test_mini.cpp
  test_pattern.cpp
  test_rational.cpp
)
target_link_libraries(undertow_tests PRIVATE undertow::engine)
target_compile_definitions(undertow_tests PRIVATE UNDERTOW_CONFORMANCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/conformance")

if(MSVC)
  target_compile_options(undertow_tests PRIVATE /W4)
else()
  target_compile_options(undertow_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME undertow_tests COMMAND undertow_tests)
