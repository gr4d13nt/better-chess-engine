#include "engine/attacks.hpp"
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

bool test_v21_matches_v15_at_startpos() {
  Board board;
  board.set_startpos();
  return expect_eq(search_common::evaluate_v21(board), search_common::evaluate_v15(board),
                   "v21 equals v15 at startpos (no passed pawns)");
}

bool test_white_passed_pawn_bonus() {
  Board board;
  if (!board.set_fen("4k3/8/8/3P4/8/8/8/4K3 w - - 0 1")) {
    return expect_true(false, "white passed pawn FEN parse");
  }
  return expect_gt(search_common::evaluate_v21(board), search_common::evaluate_v15(board),
                   "white passed pawn gets extra score in v21");
}

bool test_v21_passed_matches_v22_no_double_count() {
  Board board;
  if (!board.set_fen("4k3/8/8/4P3/8/8/8/4K3 w - - 0 1")) {
    return expect_true(false, "passed pawn FEN parse");
  }
  return expect_eq(search_common::evaluate_v21(board), search_common::evaluate_v22(board),
                   "v21 and v22 agree on lone passed pawn (no layered passed EG)");
}

bool test_black_passed_pawn_penalty_for_white() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/3p4/8/8/4K3 w - - 0 1")) {
    return expect_true(false, "black passed pawn FEN parse");
  }
  return expect_gt(search_common::evaluate_v15(board), search_common::evaluate_v21(board),
                   "black passed pawn hurts white eval more in v21");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_v21_matches_v15_at_startpos() && ok;
  ok = engine::test_white_passed_pawn_bonus() && ok;
  ok = engine::test_v21_passed_matches_v22_no_double_count() && ok;
  ok = engine::test_black_passed_pawn_penalty_for_white() && ok;

  if (ok) {
    std::cout << "eval_v21_tests: all passed\n";
    return 0;
  }
  std::cerr << "eval_v21_tests: failed\n";
  return 1;
}

