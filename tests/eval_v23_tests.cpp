#include "engine/board.hpp"
#include "search_common.hpp"

#include <iostream>

namespace engine {
namespace {

bool expect_true(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << '\n';
    return false;
  }
  return true;
}

bool expect_eq(int a, int b, const char* msg) {
  if (a != b) {
    std::cerr << "FAIL: " << msg << " (got " << a << " vs " << b << ")\n";
    return false;
  }
  return true;
}

bool expect_gt(int a, int b, const char* msg) {
  if (!(a > b)) {
    std::cerr << "FAIL: " << msg << " (got " << a << " vs " << b << ")\n";
    return false;
  }
  return true;
}

bool test_v23_matches_v22_at_startpos() {
  Board board;
  board.set_startpos();
  return expect_eq(search_common::evaluate_v23(board), search_common::evaluate_v22(board),
                   "v23 equals v22 at startpos (symmetric space)");
}

bool test_v23_space_after_e4() {
  Board board;
  if (!board.set_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1")) {
    return expect_true(false, "e4 FEN parse");
  }
  return expect_gt(search_common::evaluate_v23(board), search_common::evaluate_v22(board),
                   "e4 central pawn improves white eval in v23");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_v23_matches_v22_at_startpos() && ok;
  ok = engine::test_v23_space_after_e4() && ok;

  if (ok) {
    std::cout << "eval_v23_tests: all passed\n";
    return 0;
  }
  std::cerr << "eval_v23_tests: failed\n";
  return 1;
}
