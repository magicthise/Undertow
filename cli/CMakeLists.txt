add_executable(undertow-render undertow_render.cpp)
target_link_libraries(undertow-render PRIVATE undertow::engine)

if(MSVC)
  target_compile_options(undertow-render PRIVATE /W4)
else()
  target_compile_options(undertow-render PRIVATE -Wall -Wextra -Wpedantic)
endif()
