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
  src/repetition.cpp
  src/zobrist.cpp
  src/search/search_common.cpp
  src/search/search_dispatch.cpp
  src/search/search_tt.cpp
  src/search/search_v1_no_pruning.cpp
  src/search/search_v2_alpha_beta.cpp
  src/search/search_v4_quiescence.cpp
  src/search/search_v5_move_ordering.cpp
  src/search/search_v10_tt.cpp
  src/search/search_v11_tt.cpp
  src/search/search_v12_tt.cpp
  src/search/search_v14_tt.cpp
  src/search/search_ordering.cpp
  src/search/search_eval_phase.cpp
  src/search/search_eval_layered.cpp
  src/search/search_eval_v6.cpp
  src/search/search_eval_v7.cpp
  src/search/search_eval_knight.cpp
  src/search/search_eval_v8.cpp
  src/search/search_eval_rook.cpp
  src/search/search_eval_v9.cpp
  src/search/search_eval_mopup.cpp
  src/search/search_eval_v13.cpp
)

target_include_directories(engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src/search
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

add_executable(gui_server src/gui_server.cpp)
target_include_directories(gui_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(gui_server PRIVATE CHESS_WEB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/web")
target_link_libraries(gui_server PRIVATE engine)

enable_testing()
add_executable(perft_tests tests/perft_tests.cpp)
target_link_libraries(perft_tests PRIVATE engine)
add_test(NAME perft COMMAND perft_tests)

add_executable(eval_v6_tests tests/eval_v6_tests.cpp)
target_link_libraries(eval_v6_tests PRIVATE engine)
add_test(NAME eval_v6 COMMAND eval_v6_tests)

add_executable(eval_v7_tests tests/eval_v7_tests.cpp)
target_link_libraries(eval_v7_tests PRIVATE engine)
add_test(NAME eval_v7 COMMAND eval_v7_tests)

add_executable(eval_v8_tests tests/eval_v8_tests.cpp)
target_link_libraries(eval_v8_tests PRIVATE engine)
add_test(NAME eval_v8 COMMAND eval_v8_tests)

add_executable(eval_v9_tests tests/eval_v9_tests.cpp)
target_link_libraries(eval_v9_tests PRIVATE engine)
add_test(NAME eval_v9 COMMAND eval_v9_tests)

add_executable(eval_v13_tests tests/eval_v13_tests.cpp)
target_link_libraries(eval_v13_tests PRIVATE engine)
add_test(NAME eval_v13 COMMAND eval_v13_tests)

add_executable(repetition_tests tests/repetition_tests.cpp)
target_link_libraries(repetition_tests PRIVATE engine)
add_test(NAME repetition COMMAND repetition_tests)

add_executable(tt_tests tests/tt_tests.cpp)
target_link_libraries(tt_tests PRIVATE engine)
add_test(NAME tt COMMAND tt_tests)
