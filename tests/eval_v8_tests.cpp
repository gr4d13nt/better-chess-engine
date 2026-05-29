#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "search_common.hpp"
#include "search_eval_knight.hpp"

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

bool expect_gt(int a, int b, const char* msg) {
  if (!(a > b)) {
    std::cerr << "FAIL: " << msg << " (got " << a << " vs " << b << ")\n";
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

bool test_supported_outpost_beats_v7() {
  Board board;
  if (!board.set_fen("4k3/8/8/3N4/3P4/8/3P4/4K3 w - - 0 1")) {
    return expect_true(false, "supported outpost FEN parse");
  }
  const int v7 = search_common::evaluate_v7(board);
  const int v8 = search_common::evaluate_v8(board);
  return expect_gt(v8, v7, "supported Nd5 outpost scores above v7");
}

bool test_pawn_attack_removes_outpost_bonus() {
  Board outpost;
  Board kicked;
  if (!outpost.set_fen("4k3/8/8/3N4/3P4/8/3P4/4K3 w - - 0 1")) {
    return expect_true(false, "outpost FEN parse");
  }
  if (!kicked.set_fen("4k3/4p3/3p4/3N4/8/8/3P4/4K3 w - - 0 1")) {
    return expect_true(false, "kicked knight FEN parse");
  }
  const int outpost_v8 = search_common::evaluate_v8(outpost);
  const int kicked_v8 = search_common::evaluate_v8(kicked);
  const int kicked_v7 = search_common::evaluate_v7(kicked);
  return expect_gt(outpost_v8, kicked_v8, "pawn-attacked knight loses outpost value") &&
         expect_eq(kicked_v8, kicked_v7, "pawn-attacked knight matches v7");
}

bool test_outpost_split_nonzero() {
  Board board;
  if (!board.set_fen("4k3/8/8/3N4/3P4/8/3P4/4K3 w - - 0 1")) {
    return expect_true(false, "outpost FEN parse");
  }
  int mg_w = 0;
  int eg_w = 0;
  int mg_b = 0;
  int eg_b = 0;
  search_common::evaluate_knight_outposts_split(board, mg_w, eg_w, mg_b, eg_b);
  return expect_gt(mg_w, 20, "white knight outpost MG bonus is meaningful");
}

bool test_v8_includes_v7_tempo() {
  Board white_stm;
  Board black_stm;
  white_stm.set_startpos();
  if (!black_stm.set_fen(
          "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1")) {
    return false;
  }
  const int white_turn = search_common::evaluate_v8(white_stm);
  const int black_turn = search_common::evaluate_v8(black_stm);
  return expect_gt(white_turn, black_turn, "v8 tempo favors side to move");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_supported_outpost_beats_v7() && ok;
  ok = engine::test_pawn_attack_removes_outpost_bonus() && ok;
  ok = engine::test_outpost_split_nonzero() && ok;
  ok = engine::test_v8_includes_v7_tempo() && ok;

  if (ok) {
    std::cout << "eval_v8_tests: all passed\n";
    return 0;
  }
  std::cerr << "eval_v8_tests: failed\n";
  return 1;
}
