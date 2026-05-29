#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "search_common.hpp"
#include "search_eval_layered.hpp"

#include <iostream>

namespace engine {
namespace {

bool expect_gt(int a, int b, const char* msg) {
  if (!(a > b)) {
    std::cerr << "FAIL: " << msg << " (got " << a << " vs " << b << ")\n";
    return false;
  }
  return true;
}

bool expect_true(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << '\n';
    return false;
  }
  return true;
}

bool test_tempo_favors_side_to_move() {
  Board white_stm;
  Board black_stm;
  white_stm.set_startpos();
  if (!black_stm.set_fen(
          "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1")) {
    return false;
  }
  const int white_turn = search_common::evaluate_v7(white_stm);
  const int black_turn = search_common::evaluate_v7(black_stm);
  return expect_gt(white_turn, black_turn, "white to move scores higher than black to move");
}

bool test_v7_includes_layered_terms() {
  Board board;
  if (!board.set_fen("4k3/3ppp2/8/8/3P4/4P3/8/4K3 w - - 0 1")) {
    return false;
  }
  int imp_mg_w = 0;
  int imp_eg_w = 0;
  int imp_mg_b = 0;
  int imp_eg_b = 0;
  search_common::evaluate_improved_split(board, imp_mg_w, imp_eg_w, imp_mg_b, imp_eg_b);

  int lay_mg_w = 0;
  int lay_eg_w = 0;
  int lay_mg_b = 0;
  int lay_eg_b = 0;
  search_common::evaluate_layered_split(board, lay_mg_w, lay_eg_w, lay_mg_b, lay_eg_b);

  const bool has_layered = (lay_mg_w - lay_mg_b) != 0 || (lay_eg_w - lay_eg_b) != 0;
  return expect_true(has_layered, "layered pawn/king terms are non-zero");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_tempo_favors_side_to_move() && ok;
  ok = engine::test_v7_includes_layered_terms() && ok;

  if (ok) {
    std::cout << "eval_v7_tests: all passed\n";
    return 0;
  }
  std::cerr << "eval_v7_tests: failed\n";
  return 1;
}
