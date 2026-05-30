#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "search_common.hpp"
#include "search_eval_bishop.hpp"

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

bool test_v15_matches_v13_at_startpos() {
  Board board;
  board.set_startpos();
  return expect_eq(search_common::evaluate_v15(board), search_common::evaluate_v13(board),
                   "v15 equals v13 at startpos (no piece bonuses yet)");
}

bool test_supported_outpost_beats_v13() {
  Board board;
  if (!board.set_fen("4k3/8/8/3N4/3P4/8/3P4/4K3 w - - 0 1")) {
    return expect_true(false, "supported outpost FEN parse");
  }
  return expect_gt(search_common::evaluate_v15(board), search_common::evaluate_v13(board),
                   "supported Nd5 outpost scores above v13");
}

bool test_open_file_rook_beats_v13() {
  Board open;
  Board closed;
  if (!open.set_fen("4k3/8/8/8/4R3/8/3P4/4K3 w - - 0 1")) {
    return expect_true(false, "open file FEN parse");
  }
  if (!closed.set_fen("4k3/8/8/8/4R3/8/4P3/4K3 w - - 0 1")) {
    return expect_true(false, "closed file FEN parse");
  }
  const int open_v15 = search_common::evaluate_v15(open);
  const int closed_v15 = search_common::evaluate_v15(closed);
  return expect_gt(open_v15, closed_v15, "open file rook beats own-pawn blocked file");
}

bool test_free_bishop_beats_blocked() {
  Board free;
  Board blocked;
  if (!free.set_fen("8/8/4k3/8/4P3/4B3/8/4K3 w - - 0 1")) {
    return expect_true(false, "free bishop FEN parse");
  }
  if (!blocked.set_fen("8/8/4k3/8/5P2/4B3/8/4K3 w - - 0 1")) {
    return expect_true(false, "blocked bishop FEN parse");
  }
  return expect_gt(search_common::evaluate_v15(free), search_common::evaluate_v15(blocked),
                   "bishop with fewer same-color pawns scores higher");
}

bool test_bishop_split_nonzero() {
  Board board;
  if (!board.set_fen("8/8/4k3/8/5P2/4B3/8/4K3 w - - 0 1")) {
    return expect_true(false, "blocked bishop FEN parse");
  }
  int mg_w = 0;
  int eg_w = 0;
  int mg_b = 0;
  int eg_b = 0;
  search_common::evaluate_bishop_freedom_split(board, mg_w, eg_w, mg_b, eg_b);
  return expect_true(mg_w < 0, "blocked bishop MG penalty is applied");
}

bool test_v15_beats_v13_in_winning_rook_endgame() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/4R3/8/3P4/4K3 w - - 0 1")) {
    return expect_true(false, "K+R vs K FEN parse");
  }
  return expect_gt(search_common::evaluate_v15(board), search_common::evaluate_v13(board),
                   "v15 scores higher than v13 with rook on open file + mop-up");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_v15_matches_v13_at_startpos() && ok;
  ok = engine::test_supported_outpost_beats_v13() && ok;
  ok = engine::test_open_file_rook_beats_v13() && ok;
  ok = engine::test_free_bishop_beats_blocked() && ok;
  ok = engine::test_bishop_split_nonzero() && ok;
  ok = engine::test_v15_beats_v13_in_winning_rook_endgame() && ok;

  if (ok) {
    std::cout << "eval_v15_tests: all passed\n";
    return 0;
  }
  std::cerr << "eval_v15_tests: failed\n";
  return 1;
}
