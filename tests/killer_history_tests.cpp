#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"

#include <cmath>
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

bool test_v19_fewer_nodes_than_v18() {
  Board board;
  board.set_startpos();

  SearchConfig v18{};
  v18.depth = 6;
  v18.version = EngineVersion::V18_NullMove;

  SearchConfig v19{};
  v19.depth = 6;
  v19.version = EngineVersion::V19_KillerHistory;

  const SearchResult r18 = search_best_move(board, v18);
  const SearchResult r19 = search_best_move(board, v19);
  if (!r18.has_move || !r19.has_move) {
    return expect_true(false, "search returned no move");
  }

  return expect_true(r19.nodes <= r18.nodes,
                     "v19 searches no more nodes than v18 at depth 6 startpos");
}

bool test_v19_no_phantom_mate_score() {
  Board board;
  board.set_startpos();

  SearchConfig v19{};
  v19.depth = 6;
  v19.version = EngineVersion::V19_KillerHistory;

  const SearchResult result = search_best_move(board, v19);
  if (!result.has_move) {
    return expect_true(false, "v19 returned no move");
  }

  return expect_true(std::abs(result.score) < 50000,
                     "v19 startpos depth 6 score is not a bogus mate value");
}

bool test_v19_keeps_delta_see_pruning() {
  Board board;
  if (!board.set_fen("8/8/4n3/8/3p4/8/4N3/4K2k b - - 0 1")) {
    return expect_true(false, "losing capture FEN parse");
  }

  SearchConfig v19{};
  v19.depth = 2;
  v19.version = EngineVersion::V19_KillerHistory;

  const SearchResult result = search_best_move(board, v19);
  if (!result.has_move) {
    return expect_true(false, "v19 returned no move");
  }

  return expect_true(move_to_uci(result.best_move) != "e6d4",
                     "v19 keeps qsearch pruning from v17/v18");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();
  engine::init_zobrist();

  bool ok = true;
  ok = engine::test_v19_fewer_nodes_than_v18() && ok;
  ok = engine::test_v19_no_phantom_mate_score() && ok;
  ok = engine::test_v19_keeps_delta_see_pruning() && ok;

  if (ok) {
    std::cout << "killer_history_tests: all passed\n";
    return 0;
  }
  std::cerr << "killer_history_tests: failed\n";
  return 1;
}
