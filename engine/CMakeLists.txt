add_library(undertow_engine
  src/midi_file.cpp
  src/mini.cpp
  src/note.cpp
  src/pattern.cpp
  src/rational.cpp
  src/span.cpp
)
add_library(undertow::engine ALIAS undertow_engine)

target_include_directories(undertow_engine PUBLIC include)
target_compile_features(undertow_engine PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(undertow_engine PRIVATE /W4)
else()
  target_compile_options(undertow_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
