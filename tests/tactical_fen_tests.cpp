#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/book.hpp"
#include "engine/move.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"

#include "search_common.hpp"
#include "search_tt.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace engine {
namespace {

constexpr const char* kFen = "6k1/1pp2rp1/p2pNq2/3P4/8/3b4/PP1Q2PP/4R1K1 w - - 2 1";

bool expect_true(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << '\n';
    return false;
  }
  return true;
}

Move find_move(const Board& board, const std::string& uci) {
  MoveList moves;
  generate_legal_moves(board, moves);
  for (int i = 0; i < moves.count; ++i) {
    if (move_to_uci(moves.moves[i]) == uci) {
      return moves.moves[i];
    }
  }
  return Move{};
}

bool test_not_in_opening_book() {
  Board board;
  board.set_fen(kFen);
  init_opening_book();
  return expect_true(!probe_opening_book(board).has_value(), "midgame FEN is out of book");
}

bool test_e1d1_allows_black_mate_in_two() {
  Board board;
  board.set_fen(kFen);

  const Move rd1 = find_move(board, "e1d1");
  if (!expect_true(rd1.from != Square::None, "white can play e1d1")) {
    return false;
  }

  Undo undo_w;
  board.make_move(rd1, undo_w);

  const Move qf1 = find_move(board, "f6f1");
  if (!expect_true(qf1.from != Square::None, "black has Qf1+ after e1d1")) {
    return false;
  }

  Undo undo_b;
  board.make_move(qf1, undo_b);
  if (!expect_true(board.in_check(Color::White), "Qf1+ gives check")) {
    return false;
  }

  const Move rxf1 = find_move(board, "d1f1");
  if (!expect_true(rxf1.from != Square::None, "only Rxf1 blocks Qf1+")) {
    return false;
  }

  Undo undo_r;
  board.make_move(rxf1, undo_r);

  const Move rxf1_mate = find_move(board, "f7f1");
  if (!expect_true(rxf1_mate.from != Square::None, "black has Rxf1#")) {
    return false;
  }

  Undo undo_m;
  board.make_move(rxf1_mate, undo_m);
  return expect_true(board.is_checkmate(), "Rxf1 is checkmate");
}

bool test_negamax_sees_mate_after_e1d1() {
  Board board;
  board.set_fen(kFen);

  const Move rd1 = find_move(board, "e1d1");
  Undo undo;
  board.make_move(rd1, undo);

  search_common::tt_clear();
  search_common::SearchState st{};
  st.eval_profile = EngineVersion::V24_HangingPieces;
  st.tt = &search_common::global_tt();
  st.tt_generation = st.tt->new_search();

  int child_score = 0;
  const bool ok =
      search_common::negamax_v24_tt(board, 7, -search_common::kMateScore, search_common::kMateScore,
                                    st, child_score);
  if (!expect_true(ok, "negamax completes after e1d1")) {
    return false;
  }

  std::cout << "negamax after e1d1 (black to move) score=" << child_score << '\n';
  return expect_true(child_score > search_common::kMateScore / 2,
                     "black should see a winning forced line after e1d1");
}

bool test_narrow_window_still_finds_mate() {
  Board board;
  board.set_fen(kFen);

  const Move rd1 = find_move(board, "e1d1");
  Undo undo;
  board.make_move(rd1, undo);

  search_common::tt_clear();
  search_common::SearchState st{};
  st.eval_profile = EngineVersion::V24_HangingPieces;
  st.tt = &search_common::global_tt();
  st.tt_generation = st.tt->new_search();

  int narrow_score = 0;
  const bool ok = search_common::negamax_v24_tt(
      board, 7, -search_common::kMateScore, -124, st, narrow_score);
  if (!expect_true(ok, "narrow-window negamax completes")) {
    return false;
  }
  std::cout << "narrow-window score after e1d1=" << narrow_score << '\n';
  return expect_true(narrow_score > search_common::kMateScore / 2,
                     "narrow beta must not hide forced mate");
}

bool test_search_avoids_rd1_blunder_at_depth_8() {
  Board board;
  board.set_fen(kFen);

  search_common::tt_clear();

  SearchConfig cfg{};
  cfg.depth = 8;
  cfg.version = EngineVersion::V25_OpeningBook;
  cfg.movetime_ms = 0;

  const SearchResult result = search_best_move(board, cfg);
  if (!expect_true(result.has_move, "search returns a move")) {
    return false;
  }

  const std::string uci = move_to_uci(result.best_move);
  std::cout << "depth-8 best: " << uci << " score=" << result.score << " nodes=" << result.nodes
            << '\n';

  return expect_true(uci != "e1d1", "search should not play Rd1 (allows Qf1+ Rxf1#)");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();
  engine::init_zobrist();

  bool ok = true;
  ok = engine::test_not_in_opening_book() && ok;
  ok = engine::test_e1d1_allows_black_mate_in_two() && ok;
  ok = engine::test_negamax_sees_mate_after_e1d1() && ok;
  ok = engine::test_narrow_window_still_finds_mate() && ok;
  ok = engine::test_search_avoids_rd1_blunder_at_depth_8() && ok;

  if (ok) {
    std::cout << "tactical_fen_tests: all passed\n";
    return 0;
  }
  std::cerr << "tactical_fen_tests: failed\n";
  return 1;
}
