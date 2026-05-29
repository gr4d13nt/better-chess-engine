#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "search_common.hpp"
#include "search_eval_rook.hpp"

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

bool test_open_file_rook_beats_v8() {
  Board open;
  Board closed;
  if (!open.set_fen("4k3/8/8/8/4R3/8/3P4/4K3 w - - 0 1")) {
    return expect_true(false, "open file FEN parse");
  }
  if (!closed.set_fen("4k3/8/8/8/4R3/8/4P3/4K3 w - - 0 1")) {
    return expect_true(false, "closed file FEN parse");
  }
  const int open_v9 = search_common::evaluate_v9(open);
  const int open_v8 = search_common::evaluate_v8(open);
  const int closed_v9 = search_common::evaluate_v9(closed);
  return expect_gt(open_v9, open_v8, "rook on open e-file scores above v8") &&
         expect_gt(open_v9, closed_v9, "open file rook beats own-pawn blocked file");
}

bool test_seventh_rank_uses_pst_only() {
  Board seventh;
  Board fourth;
  if (!seventh.set_fen("4k3/4R3/8/8/8/8/8/4K3 w - - 0 1")) {
    return expect_true(false, "seventh rank FEN parse");
  }
  if (!fourth.set_fen("4k3/8/8/8/4R3/8/8/4K3 w - - 0 1")) {
    return expect_true(false, "fourth rank FEN parse");
  }
  const int v9_delta = search_common::evaluate_v9(seventh) - search_common::evaluate_v9(fourth);
  const int v8_delta = search_common::evaluate_v8(seventh) - search_common::evaluate_v8(fourth);
  return expect_eq(v9_delta, v8_delta, "7th vs 4th on open e-file: v9 adds no extra rank bonus");
}

bool test_home_rank_rook_gets_no_bonus() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/8/8/6R1/4K3 w - - 0 1")) {
    return expect_true(false, "home rank rook FEN parse");
  }
  return expect_eq(search_common::evaluate_v9(board), search_common::evaluate_v8(board),
                   "undeveloped rook on h2 matches v8");
}

bool test_closed_file_matches_v8() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/4R3/8/4P3/4K3 w - - 0 1")) {
    return expect_true(false, "closed file FEN parse");
  }
  return expect_eq(search_common::evaluate_v9(board), search_common::evaluate_v8(board),
                   "rook behind own pawn matches v8");
}

bool test_rook_split_nonzero() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/4R3/8/3P4/4K3 w - - 0 1")) {
    return expect_true(false, "open file FEN parse");
  }
  int mg_w = 0;
  int eg_w = 0;
  int mg_b = 0;
  int eg_b = 0;
  search_common::evaluate_rook_placement_split(board, mg_w, eg_w, mg_b, eg_b);
  return expect_gt(mg_w, 0, "white rook placement MG bonus is positive");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_open_file_rook_beats_v8() && ok;
  ok = engine::test_seventh_rank_uses_pst_only() && ok;
  ok = engine::test_closed_file_matches_v8() && ok;
  ok = engine::test_rook_split_nonzero() && ok;
  ok = engine::test_home_rank_rook_gets_no_bonus() && ok;

  if (ok) {
    std::cout << "eval_v9_tests: all passed\n";
    return 0;
  }
  std::cerr << "eval_v9_tests: failed\n";
  return 1;
}
