cmake_minimum_required(VERSION 3.16)
project(chess_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(engine STATIC
  src/attacks.cpp
  src/board.cpp
  src/movegen.cpp
  src/search/search_common.cpp
  src/search/search_dispatch.cpp
  src/search/search_v1_no_pruning.cpp
  src/search/search_v2_alpha_beta.cpp
  src/search/search_v4_quiescence.cpp
  src/search/search_v5_move_ordering.cpp
)

target_include_directories(engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(engine PRIVATE /W4 /permissive-)
else()
  target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(perft src/perft_main.cpp)
target_link_libraries(perft PRIVATE engine)

add_executable(search_main src/search_main.cpp)
target_link_libraries(search_main PRIVATE engine)

add_executable(selfplay_main src/selfplay_main.cpp)
target_link_libraries(selfplay_main PRIVATE engine)

enable_testing()
add_executable(perft_tests tests/perft_tests.cpp)
target_link_libraries(perft_tests PRIVATE engine)
add_test(NAME perft COMMAND perft_tests)
