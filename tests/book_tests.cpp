#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/book.hpp"
#include "engine/move.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"

#include <iostream>
#include <set>
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

bool is_book_first_move(const std::string& uci) {
  static const std::set<std::string> kFirstMoves = {"e2e4", "d2d4", "c2c4", "g1f3"};
  return kFirstMoves.count(uci) > 0;
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();
  engine::init_zobrist();
  engine::init_opening_book();

  bool ok = true;
  ok &= engine::expect_true(engine::opening_book_loaded(), "opening book should load");
  ok &= engine::expect_true(engine::opening_book_entry_count() > 1000,
                              "polyglot book should have many entries");

  engine::Board board;
  board.set_startpos();

  const auto probe = engine::probe_opening_book(board);
  ok &= engine::expect_true(probe.has_value(), "startpos should be in book");
  if (probe.has_value()) {
    const std::string uci = engine::move_to_uci(*probe);
    ok &= engine::expect_true(engine::is_book_first_move(uci),
                              "book first move should be e4, d4, c4, or Nf3");
  }

  engine::SearchConfig cfg{};
  cfg.depth = 4;
  cfg.version = engine::EngineVersion::V25_OpeningBook;
  cfg.use_opening_book = true;

  board.set_startpos();
  const engine::SearchResult start = engine::search_best_move(board, cfg);
  ok &= engine::expect_true(start.has_move, "v25 should move from startpos");
  ok &= engine::expect_true(start.nodes == 0, "v25 should use book from startpos");

  const engine::Move e4 = engine::find_move(board, "e2e4");
  engine::Undo undo;
  board.make_move(e4, undo);
  const auto reply = engine::probe_opening_book(board);
  ok &= engine::expect_true(reply.has_value(), "after 1.e4 book should reply");
  const engine::SearchResult after_e4 = engine::search_best_move(board, cfg);
  ok &= engine::expect_true(after_e4.has_move, "v25 should move after 1.e4");
  ok &= engine::expect_true(after_e4.nodes == 0, "v25 should stay on book after 1.e4");

  board.set_startpos();
  cfg.version = engine::EngineVersion::V24_HangingPieces;
  const engine::SearchResult searched = engine::search_best_move(board, cfg);
  ok &= engine::expect_true(searched.has_move, "search without book should still move");
  ok &= engine::expect_true(searched.nodes > 0, "search without book should search nodes");

  if (!ok) {
    return 1;
  }
  std::cout << "book_tests: all passed\n";
  return 0;
}
