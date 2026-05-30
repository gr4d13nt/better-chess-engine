#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/movegen.hpp"
#include "engine/repetition.hpp"
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

bool test_v14_avoids_repeated_move() {
  Board board;
  if (!board.set_fen("8/4k3/8/1R6/4K3/8/8/8 w - - 0 1")) {
    return expect_true(false, "K+R FEN parse");
  }
  const std::uint64_t current_hash = board.zobrist();

  Move ke5 = find_move(board, "e4e5");
  if (ke5.from == Square::None) {
    return expect_true(false, "e4e5 exists");
  }

  Undo undo;
  board.make_move(ke5, undo);
  const std::uint64_t after_ke5 = board.zobrist();
  board.unmake_move(ke5, undo);

  SearchConfig v14{};
  v14.depth = 4;
  v14.version = EngineVersion::V14_RepetitionDraw;
  v14.repetition_history = {current_hash, after_ke5, current_hash};

  const SearchResult r14 = search_best_move(board, v14);
  if (!expect_true(r14.has_move, "v14 search returns a move")) {
    return false;
  }

  return expect_ne(move_to_uci(r14.best_move), "e4e5",
                   "v14 avoids king shuffle back to a repeated position");
}

bool test_repetition_hashes_from_fens() {
  const std::vector<std::string> fens = {"startpos", "startpos"};
  const std::vector<std::uint64_t> hashes = repetition_hashes_from_fens(fens);
  return expect_true(hashes.size() == 2 && hashes[0] == hashes[1],
                     "startpos fens produce identical zobrist hashes");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();
  engine::init_zobrist();

  bool ok = true;
  ok = engine::test_repetition_hashes_from_fens() && ok;
  ok = engine::test_v14_avoids_repeated_move() && ok;

  if (ok) {
    std::cout << "repetition_tests: all passed\n";
    return 0;
  }
  std::cerr << "repetition_tests: failed\n";
  return 1;
}
