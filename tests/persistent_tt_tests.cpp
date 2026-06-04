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

bool test_v19_clears_tt_each_search() {
  Board board;
  board.set_startpos();

  SearchConfig cfg{};
  cfg.depth = 6;
  cfg.version = EngineVersion::V19_KillerHistory;

  const SearchResult first = search_best_move(board, cfg);
  const SearchResult second = search_best_move(board, cfg);
  if (!first.has_move || !second.has_move) {
    return expect_true(false, "v19 search returned no move");
  }

  const int lo = static_cast<int>(static_cast<double>(first.nodes) * 0.85);
  const int hi = static_cast<int>(static_cast<double>(first.nodes) * 1.15);
  return expect_true(second.nodes >= lo && second.nodes <= hi,
                     "v19 repeats similar node count when TT is cleared each search");
}

bool test_v20_warm_tt_hash_move_help() {
  clear_transposition_table();

  Board board;
  board.set_startpos();

  SearchConfig cfg{};
  cfg.depth = 6;
  cfg.version = EngineVersion::V20_PersistentTT;
  cfg.clear_transposition_table = true;

  const SearchResult cold = search_best_move(board, cfg);
  cfg.clear_transposition_table = false;
  const SearchResult warm = search_best_move(board, cfg);
  if (!cold.has_move || !warm.has_move) {
    return expect_true(false, "v20 search returned no move");
  }

  return expect_true(warm.nodes <= cold.nodes && move_to_uci(warm.best_move) == move_to_uci(cold.best_move),
                     "v20 warm TT matches cold move and does not search more nodes");
}

bool test_v20_matches_v19_after_one_move() {
  clear_transposition_table();

  Board board;
  board.set_startpos();

  SearchConfig v19{};
  v19.depth = 6;
  v19.version = EngineVersion::V19_KillerHistory;

  SearchConfig v20{};
  v20.depth = 6;
  v20.version = EngineVersion::V20_PersistentTT;
  v20.clear_transposition_table = true;

  const SearchResult start19 = search_best_move(board, v19);
  if (!start19.has_move) {
    return expect_true(false, "v19 startpos search failed");
  }

  const std::uint64_t start_hash = board.zobrist();
  engine::Undo undo;
  board.make_move(start19.best_move, undo);

  v19.repetition_history = {start_hash, board.zobrist()};
  v20.repetition_history = v19.repetition_history;
  v20.clear_transposition_table = false;

  const SearchResult mid19 = search_best_move(board, v19);
  const SearchResult mid20 = search_best_move(board, v20);
  board.unmake_move(start19.best_move, undo);
  if (!mid19.has_move || !mid20.has_move) {
    return expect_true(false, "midgame search failed");
  }

  return expect_true(mid19.score == mid20.score && move_to_uci(mid19.best_move) == move_to_uci(mid20.best_move),
                     "v20 matches v19 after prior ply filled persistent TT");
}

bool test_v20_clear_tt_resets() {
  clear_transposition_table();

  Board board;
  board.set_startpos();

  SearchConfig cfg{};
  cfg.depth = 6;
  cfg.version = EngineVersion::V20_PersistentTT;

  cfg.clear_transposition_table = false;
  const SearchResult warm = search_best_move(board, cfg);

  cfg.clear_transposition_table = true;
  const SearchResult cleared = search_best_move(board, cfg);
  if (!warm.has_move || !cleared.has_move) {
    return expect_true(false, "v20 search returned no move");
  }

  const int lo = static_cast<int>(static_cast<double>(warm.nodes) * 0.85);
  const int hi = static_cast<int>(static_cast<double>(warm.nodes) * 1.15);
  return expect_true(cleared.nodes >= lo && cleared.nodes <= hi,
                     "v20 clear_transposition_table resets TT like a cold search");
}

bool test_v20_same_score_as_v19_startpos() {
  clear_transposition_table();

  Board board;
  board.set_startpos();

  SearchConfig v19{};
  v19.depth = 6;
  v19.version = EngineVersion::V19_KillerHistory;

  SearchConfig v20{};
  v20.depth = 6;
  v20.version = EngineVersion::V20_PersistentTT;
  v20.clear_transposition_table = true;

  const SearchResult r19 = search_best_move(board, v19);
  const SearchResult r20 = search_best_move(board, v20);
  if (!r19.has_move || !r20.has_move) {
    return expect_true(false, "search returned no move");
  }

  return expect_true(r19.score == r20.score && move_to_uci(r19.best_move) == move_to_uci(r20.best_move),
                     "v20 cold search matches v19 move/score at depth 6 startpos");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();
  engine::init_zobrist();

  bool ok = true;
  ok = engine::test_v19_clears_tt_each_search() && ok;
  ok = engine::test_v20_warm_tt_hash_move_help() && ok;
  ok = engine::test_v20_matches_v19_after_one_move() && ok;
  ok = engine::test_v20_clear_tt_resets() && ok;
  ok = engine::test_v20_same_score_as_v19_startpos() && ok;

  if (ok) {
    std::cout << "persistent_tt_tests: all passed\n";
    return 0;
  }
  std::cerr << "persistent_tt_tests: failed\n";
  return 1;
}
