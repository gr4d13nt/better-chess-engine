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

bool expect_lt(int a, int b, const char* msg) {
  if (!(a < b)) {
    std::cerr << "FAIL: " << msg << " (got " << a << " vs " << b << ")\n";
    return false;
  }
  return true;
}

bool test_v24_matches_v23_at_startpos() {
  Board board;
  board.set_startpos();
  return expect_eq(search_common::evaluate_v24(board), search_common::evaluate_v23(board),
                   "v24 equals v23 at startpos (no hanging pieces)");
}

bool test_v24_hanging_rook_penalty() {
  Board safe;
  Board hanging;
  if (!safe.set_fen("4k3/8/8/8/8/8/8/R6K w - - 0 1")) {
    return expect_true(false, "safe rook FEN");
  }
  if (!hanging.set_fen("4k3/8/8/8/8/2b5/8/R6K w - - 0 1")) {
    return expect_true(false, "hanging rook FEN");
  }
  return expect_lt(search_common::evaluate_v24(hanging), search_common::evaluate_v24(safe),
                   "undefended rook on a1 scores lower than safe rook");
}

bool test_v24_below_v23_on_hanging_only() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/8/2b5/8/R6K w - - 0 1")) {
    return expect_true(false, "hanging rook FEN");
  }
  return expect_lt(search_common::evaluate_v24(board), search_common::evaluate_v23(board),
                   "v24 penalizes hanging rook beyond v23");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_v24_matches_v23_at_startpos() && ok;
  ok = engine::test_v24_hanging_rook_penalty() && ok;
  ok = engine::test_v24_below_v23_on_hanging_only() && ok;

  if (ok) {
    std::cout << "eval_v24_tests: all passed\n";
    return 0;
  }
  std::cerr << "eval_v24_tests: failed\n";
  return 1;
}
