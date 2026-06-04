#include "engine/board.hpp"
#include "search_common.hpp"

#include <cstdlib>

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

bool expect_lt(int a, int b, const char* msg) {
  if (!(a < b)) {
    std::cerr << "FAIL: " << msg << " (got " << a << " vs " << b << ")\n";
    return false;
  }
  return true;
}

bool test_v22_matches_v21_at_startpos() {
  Board board;
  board.set_startpos();
  return expect_eq(search_common::evaluate_v22(board), search_common::evaluate_v21(board),
                   "v22 equals v21 at startpos (no extra extended pawn terms)");
}

bool test_extra_islands_penalized() {
  Board clustered;
  Board fragmented;
  if (!clustered.set_fen("4k3/8/8/PPP5/8/8/8/4K3 w - - 0 1")) {
    return expect_true(false, "clustered pawns FEN");
  }
  if (!fragmented.set_fen("4k3/8/8/8/8/P2P1P2/8/4K3 w - - 0 1")) {
    return expect_true(false, "fragmented pawns FEN");
  }
  return expect_gt(search_common::evaluate_v22(clustered),
                   search_common::evaluate_v22(fragmented),
                   "fewer pawn islands score higher for white");
}

bool test_backward_pawn_worse_than_supported() {
  Board supported;
  Board backward;
  if (!supported.set_fen("4k3/8/3P4/3P4/8/8/8/4K3 w - - 0 1")) {
    return expect_true(false, "supported pawns FEN");
  }
  if (!backward.set_fen("4k3/8/4p3/3P4/8/8/8/4K3 w - - 0 1")) {
    return expect_true(false, "backward pawn FEN");
  }
  return expect_gt(search_common::evaluate_v22(supported), search_common::evaluate_v22(backward),
                   "backward pawn structure scores lower than supported chain");
}

bool test_candidate_passed_bonus() {
  Board candidate;
  Board blocked;
  if (!candidate.set_fen("4k3/8/8/3P4/8/8/4p3/4K3 w - - 0 1")) {
    return expect_true(false, "candidate passed FEN");
  }
  if (!blocked.set_fen("4k3/8/8/3P4/8/8/3p4/4K3 w - - 0 1")) {
    return expect_true(false, "blocked pawn FEN");
  }
  return expect_gt(search_common::evaluate_v22(candidate), search_common::evaluate_v22(blocked),
                   "candidate passed pawn scores higher than blocked wing pawn");
}

bool test_v22_improves_over_v21_on_islands() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/8/P2P1P2/8/4K3 w - - 0 1")) {
    return expect_true(false, "islands FEN");
  }
  return expect_lt(search_common::evaluate_v22(board), search_common::evaluate_v21(board),
                   "v22 island penalty lowers white eval vs v21 on fragmented pawns");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_v22_matches_v21_at_startpos() && ok;
  ok = engine::test_extra_islands_penalized() && ok;
  ok = engine::test_backward_pawn_worse_than_supported() && ok;
  ok = engine::test_candidate_passed_bonus() && ok;
  ok = engine::test_v22_improves_over_v21_on_islands() && ok;

  if (ok) {
    std::cout << "eval_v22_tests: all passed\n";
    return 0;
  }
  std::cerr << "eval_v22_tests: failed\n";
  return 1;
}
