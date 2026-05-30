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

bool test_v17_keeps_see_pruning() {
  Board board;
  if (!board.set_fen("8/8/4n3/8/3p4/8/4N3/4K2k b - - 0 1")) {
    return expect_true(false, "losing capture FEN parse");
  }

  SearchConfig v17{};
  v17.depth = 2;
  v17.version = EngineVersion::V17_DeltaQsearch;

  const SearchResult result = search_best_move(board, v17);
  if (!result.has_move) {
    return expect_true(false, "v17 returned no move");
  }

  return expect_ne(move_to_uci(result.best_move), "e6d4",
                   "v17 keeps SEE pruning for losing captures");
}

bool test_v17_matches_v16_on_startpos() {
  Board board;
  board.set_startpos();

  SearchConfig v16{};
  v16.depth = 4;
  v16.version = EngineVersion::V16_SeeQsearch;

  SearchConfig v17{};
  v17.depth = 4;
  v17.version = EngineVersion::V17_DeltaQsearch;

  const SearchResult r16 = search_best_move(board, v16);
  const SearchResult r17 = search_best_move(board, v17);
  if (!r16.has_move || !r17.has_move) {
    return expect_true(false, "search returned no move");
  }

  return expect_true(move_to_uci(r16.best_move) == move_to_uci(r17.best_move),
                     "v17 picks same move as v16 on startpos depth 4") &&
         expect_true(r16.score == r17.score, "v17 score matches v16 on startpos depth 4");
}

bool test_v17_searches_fewer_nodes_when_ahead() {
  Board board;
  if (!board.set_fen("6k1/6qp/8/8/8/8/7Q/6K1 w - - 0 1")) {
    return expect_true(false, "winning FEN parse");
  }

  SearchConfig v16{};
  v16.depth = 3;
  v16.version = EngineVersion::V16_SeeQsearch;

  SearchConfig v17{};
  v17.depth = 3;
  v17.version = EngineVersion::V17_DeltaQsearch;

  const SearchResult r16 = search_best_move(board, v16);
  const SearchResult r17 = search_best_move(board, v17);
  if (!r16.has_move || !r17.has_move) {
    return expect_true(false, "search returned no move");
  }

  return expect_true(r17.nodes <= r16.nodes,
                     "v17 delta pruning reduces nodes when comfortably ahead");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();
  engine::init_zobrist();

  bool ok = true;
  ok = engine::test_v17_keeps_see_pruning() && ok;
  ok = engine::test_v17_matches_v16_on_startpos() && ok;
  ok = engine::test_v17_searches_fewer_nodes_when_ahead() && ok;

  if (ok) {
    std::cout << "delta_qsearch_tests: all passed\n";
    return 0;
  }
  std::cerr << "delta_qsearch_tests: failed\n";
  return 1;
}
