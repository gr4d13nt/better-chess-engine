#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"

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

bool expect_ne(const std::string& a, const std::string& b, const char* msg) {
  if (a == b) {
    std::cerr << "FAIL: " << msg << " (both were " << a << ")\n";
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

bool test_v16_avoids_losing_capture_at_depth_two() {
  Board board;
  if (!board.set_fen("8/8/4n3/8/3p4/8/4N3/4K2k b - - 0 1")) {
    return expect_true(false, "losing capture FEN parse");
  }

  SearchConfig v16{};
  v16.depth = 2;
  v16.version = EngineVersion::V16_SeeQsearch;

  const SearchResult r16 = search_best_move(board, v16);
  if (!r16.has_move) {
    return expect_true(false, "search returned no move");
  }

  return expect_ne(move_to_uci(r16.best_move), "e6d4", "v16 avoids losing knight capture at depth 2");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();
  engine::init_zobrist();

  bool ok = true;
  ok = engine::test_v16_avoids_losing_capture_at_depth_two() && ok;

  if (ok) {
    std::cout << "see_qsearch_tests: all passed\n";
    return 0;
  }
  std::cerr << "see_qsearch_tests: failed\n";
  return 1;
}
