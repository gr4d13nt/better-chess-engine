#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"

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

bool test_null_move_toggles_side() {
  Board board;
  board.set_startpos();
  const Color before = board.side_to_move();

  NullUndo undo;
  board.make_null_move(undo);
  if (!expect_true(board.side_to_move() != before, "null move flips side")) {
    return false;
  }
  board.unmake_null_move(undo);
  return expect_true(board.side_to_move() == before, "null move unmake restores side");
}

bool test_v18_keeps_delta_see_pruning() {
  Board board;
  if (!board.set_fen("8/8/4n3/8/3p4/8/4N3/4K2k b - - 0 1")) {
    return expect_true(false, "losing capture FEN parse");
  }

  SearchConfig v18{};
  v18.depth = 2;
  v18.version = EngineVersion::V18_NullMove;

  const SearchResult result = search_best_move(board, v18);
  if (!result.has_move) {
    return expect_true(false, "v18 returned no move");
  }

  return expect_true(move_to_uci(result.best_move) != "e6d4",
                     "v18 keeps qsearch pruning from v17");
}

bool test_v18_prunes_with_null_move() {
  Board board;
  board.set_startpos();

  SearchConfig v17{};
  v17.depth = 5;
  v17.version = EngineVersion::V17_DeltaQsearch;

  SearchConfig v18{};
  v18.depth = 5;
  v18.version = EngineVersion::V18_NullMove;

  const SearchResult r17 = search_best_move(board, v17);
  const SearchResult r18 = search_best_move(board, v18);
  if (!r17.has_move || !r18.has_move) {
    return expect_true(false, "search returned no move");
  }

  return expect_true(r18.nodes < r17.nodes, "v18 null move reduces node count on startpos");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();
  engine::init_zobrist();

  bool ok = true;
  ok = engine::test_null_move_toggles_side() && ok;
  ok = engine::test_v18_keeps_delta_see_pruning() && ok;
  ok = engine::test_v18_prunes_with_null_move() && ok;

  if (ok) {
    std::cout << "null_move_tests: all passed\n";
    return 0;
  }
  std::cerr << "null_move_tests: failed\n";
  return 1;
}
