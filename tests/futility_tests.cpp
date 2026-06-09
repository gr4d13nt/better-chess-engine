#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"

#include "search_tt.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace engine {
namespace {

constexpr const char* kFens[] = {
    "6k1/1pp2rp1/p2pNq2/3P4/8/3b4/PP1Q2PP/4R1K1 w - - 2 1",
    "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
    "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
    "8/2p1pk2/p3p1p1/4P3/8/4P3/PP3PPP/2K5 w - - 0 1",
    "2kr3K/8/8/8/8/8/8/R7 w - - 0 1",
};

bool expect_true(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << '\n';
    return false;
  }
  return true;
}

SearchResult search_fen(const std::string& fen, EngineVersion version, int depth) {
  Board board;
  board.set_fen(fen);
  clear_transposition_table();

  SearchConfig cfg{};
  cfg.depth = depth;
  cfg.version = version;
  cfg.movetime_ms = 0;
  return search_best_move(board, cfg);
}

bool test_v30_matches_v29() {
  constexpr int kDepth = 5;
  bool ok = true;
  for (const char* fen : kFens) {
    const SearchResult v29 = search_fen(fen, EngineVersion::V29_LazySMP, kDepth);
    const SearchResult v30 = search_fen(fen, EngineVersion::V30_FutilityPruning, kDepth);
    if (!expect_true(v29.has_move && v30.has_move, "both versions return a move")) {
      ok = false;
      continue;
    }
    const std::string move29 = move_to_uci(v29.best_move);
    const std::string move30 = move_to_uci(v30.best_move);
    if (!expect_true(move29 == move30, "v30 best move matches v29")) {
      std::cerr << "  fen=" << fen << " v29=" << move29 << " v30=" << move30 << '\n';
      ok = false;
    }
    if (!expect_true(v29.score == v30.score, "v30 score matches v29")) {
      std::cerr << "  fen=" << fen << " v29_score=" << v29.score << " v30_score=" << v30.score
                << '\n';
      ok = false;
    }
  }
  return ok;
}

bool test_v30_searches_fewer_nodes() {
  constexpr int kDepth = 5;
  int total_v29 = 0;
  int total_v30 = 0;
  for (const char* fen : kFens) {
    total_v29 += search_fen(fen, EngineVersion::V29_LazySMP, kDepth).nodes;
    total_v30 += search_fen(fen, EngineVersion::V30_FutilityPruning, kDepth).nodes;
  }
  std::cout << "depth-5 total nodes v29=" << total_v29 << " v30=" << total_v30 << '\n';
  return expect_true(total_v30 <= total_v29, "v30 searches no more nodes than v29");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();
  engine::init_zobrist();

  bool ok = true;
  ok = engine::test_v30_matches_v29() && ok;
  ok = engine::test_v30_searches_fewer_nodes() && ok;

  if (ok) {
    std::cout << "futility_tests: all passed\n";
    return 0;
  }
  std::cerr << "futility_tests: failed\n";
  return 1;
}
