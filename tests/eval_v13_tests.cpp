#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "search_common.hpp"
#include "search_eval_mopup.hpp"

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

bool test_mop_up_zero_at_startpos() {
  Board board;
  board.set_startpos();
  return expect_eq(search_common::evaluate_mop_up_net(board), 0, "startpos has no mop-up bonus");
}

bool test_v13_matches_v7_at_startpos() {
  Board board;
  board.set_startpos();
  return expect_eq(search_common::evaluate_v13(board), search_common::evaluate_v7(board),
                   "v13 equals v7 at startpos");
}

bool test_mop_up_positive_in_winning_rook_endgame() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/8/8/4R3/4K3 w - - 0 1")) {
    return expect_true(false, "K+R vs K FEN parse");
  }
  return expect_gt(search_common::evaluate_mop_up_net(board), 0, "mop-up bonus in K+R vs K");
}

bool test_v13_beats_v7_in_winning_rook_endgame() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/8/8/4R3/4K3 w - - 0 1")) {
    return expect_true(false, "K+R vs K FEN parse");
  }
  return expect_gt(search_common::evaluate_v13(board), search_common::evaluate_v7(board),
                   "v13 scores higher than v7 in K+R vs K");
}

bool test_mop_up_prefers_closer_kings() {
  Board far;
  Board near;
  if (!far.set_fen("4k3/8/8/8/8/8/4R3/4K3 w - - 0 1")) {
    return expect_true(false, "far kings FEN parse");
  }
  if (!near.set_fen("4k3/8/8/8/8/4K3/4R3/8 w - - 0 1")) {
    return expect_true(false, "near kings FEN parse");
  }
  return expect_gt(search_common::evaluate_mop_up_net(near), search_common::evaluate_mop_up_net(far),
                   "mop-up prefers friendly king closer to enemy king");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_mop_up_zero_at_startpos() && ok;
  ok = engine::test_v13_matches_v7_at_startpos() && ok;
  ok = engine::test_mop_up_positive_in_winning_rook_endgame() && ok;
  ok = engine::test_v13_beats_v7_in_winning_rook_endgame() && ok;
  ok = engine::test_mop_up_prefers_closer_kings() && ok;

  if (ok) {
    std::cout << "eval_v13_tests: all passed\n";
    return 0;
  }
  std::cerr << "eval_v13_tests: failed\n";
  return 1;
}
