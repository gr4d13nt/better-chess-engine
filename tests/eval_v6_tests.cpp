#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "search_common.hpp"

#include <iostream>
#include <string>

namespace engine {
namespace {

bool expect_true(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << '\n';
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

bool test_passed_vs_blocked() {
  Board passed;
  Board blocked;
  if (!passed.set_fen("4k3/8/8/4P3/8/8/8/4K3 w - - 0 1")) {
    return expect_true(false, "passed pawn FEN parse");
  }
  if (!blocked.set_fen("4k3/8/4p3/4P3/8/8/8/4K3 w - - 0 1")) {
    return expect_true(false, "blocked pawn FEN parse");
  }
  const int passed_eval = search_common::evaluate_v6(passed);
  const int blocked_eval = search_common::evaluate_v6(blocked);
  return expect_gt(passed_eval, blocked_eval, "passed e-pawn scores higher than blocked by e6");
}

bool test_doubled_pawns_penalty() {
  Board supported;
  Board doubled;
  if (!supported.set_fen("4k3/3ppp2/8/8/3P4/4P3/8/4K3 w - - 0 1")) {
    return expect_true(false, "supported pawn FEN parse");
  }
  if (!doubled.set_fen("4k3/3ppp2/8/8/4P3/4P3/8/4K3 w - - 0 1")) {
    return expect_true(false, "doubled pawn FEN parse");
  }
  const int supported_pawn = search_common::evaluate_pawn_structure_net(supported);
  const int doubled_pawn = search_common::evaluate_pawn_structure_net(doubled);
  return expect_gt(supported_pawn, doubled_pawn, "c4+d4 pawn structure beats doubled d-file pawns");
}

bool test_isolated_pawn_penalty() {
  Board supported;
  Board isolated;
  if (!supported.set_fen("4k3/8/3P4/3P4/8/8/8/4K3 w - - 0 1")) {
    return false;
  }
  if (!isolated.set_fen("4k3/8/8/8/P7/8/8/4K3 w - - 0 1")) {
    return false;
  }
  const int supported_eval = search_common::evaluate_v6(supported);
  const int isolated_eval = search_common::evaluate_v6(isolated);
  return expect_gt(supported_eval, isolated_eval, "supported d/c pawns score higher than isolated a4");
}

bool test_black_passed_pawn() {
  Board passed;
  Board blocked;
  if (!passed.set_fen("4K3/8/8/8/8/4p3/8/4k3 b - - 0 1")) {
    return expect_true(false, "black passed pawn FEN parse");
  }
  if (!blocked.set_fen("4K3/8/8/8/4P3/4p3/8/4k3 b - - 0 1")) {
    return expect_true(false, "black blocked pawn FEN parse");
  }
  // Side to move black: lower eval is better for black.
  const int passed_eval = search_common::evaluate_v6(passed);
  const int blocked_eval = search_common::evaluate_v6(blocked);
  return expect_lt(passed_eval, blocked_eval, "black passed pawn lowers white-oriented eval vs blocked");
}

bool test_unique_king_attackers() {
  Board one_rook;
  Board two_rooks;
  // White king d1; black rooks on d2/f2 attack the king zone (unique attacker counting).
  if (!one_rook.set_fen("4k3/8/8/8/8/8/4r3/3K4 w - - 0 1")) {
    return expect_true(false, "one rook attack FEN parse");
  }
  if (!two_rooks.set_fen("4k3/8/8/8/8/8/r3r3/3K4 w - - 0 1")) {
    return expect_true(false, "two rook attack FEN parse");
  }
  const int one = search_common::evaluate_king_safety_net(one_rook);
  const int two = search_common::evaluate_king_safety_net(two_rooks);
  return expect_gt(one, two, "one rook near king penalized less than two unique rooks");
}

bool test_king_shield_bonus() {
  Board shielded;
  Board bare;
  if (!shielded.set_fen("5k2/PPP5/8/8/8/8/8/K6R w - - 0 1")) {
    return expect_true(false, "shielded king FEN parse");
  }
  if (!bare.set_fen("5k2/8/8/8/8/8/8/K6R w - - 0 1")) {
    return expect_true(false, "bare king FEN parse");
  }
  const int shielded_eval = search_common::evaluate_v6(shielded);
  const int bare_eval = search_common::evaluate_v6(bare);
  return expect_gt(shielded_eval, bare_eval, "white king with pawn shield scores higher than bare king");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_passed_vs_blocked() && ok;
  ok = engine::test_doubled_pawns_penalty() && ok;
  ok = engine::test_isolated_pawn_penalty() && ok;
  ok = engine::test_black_passed_pawn() && ok;
  ok = engine::test_unique_king_attackers() && ok;
  ok = engine::test_king_shield_bonus() && ok;

  if (ok) {
    std::cout << "eval_v6_tests: all passed\n";
    return 0;
  }
  std::cerr << "eval_v6_tests: failed\n";
  return 1;
}
