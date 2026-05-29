#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"
#include "search_tt.hpp"

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

bool expect_eq(std::uint64_t a, std::uint64_t b, const char* msg) {
  if (a != b) {
    std::cerr << "FAIL: " << msg << " (got " << a << " vs " << b << ")\n";
    return false;
  }
  return true;
}

bool expect_ne(std::uint64_t a, std::uint64_t b, const char* msg) {
  if (a == b) {
    std::cerr << "FAIL: " << msg << '\n';
    return false;
  }
  return true;
}

bool test_zobrist_stable_and_move_changes_hash() {
  Board board;
  board.set_startpos();
  const std::uint64_t start = board.zobrist();
  const std::uint64_t start2 = zobrist_hash(board);
  if (!expect_eq(start, start2, "board.zobrist matches zobrist_hash")) {
    return false;
  }

  MoveList moves;
  generate_legal_moves(board, moves);
  if (moves.count == 0) {
    return expect_true(false, "startpos has legal moves");
  }

  Undo undo;
  board.make_move(moves.moves[0], undo);
  return expect_ne(board.zobrist(), start, "hash changes after a move");
}

bool test_tt_probe_store() {
  search_common::TranspositionTable table(12);
  table.clear();

  const std::uint64_t key = 0x123456789ABCDEF0ULL;
  int score_out = 0;
  if (!expect_true(!table.probe(key, 4, -100, 100, score_out), "empty TT misses")) {
    return false;
  }

  table.store(key, 4, 42, search_common::TTBound::Exact);
  if (!expect_true(table.probe(key, 4, -100, 100, score_out), "stored TT hits")) {
    return false;
  }
  return expect_true(score_out == 42, "stored score matches");
}

bool test_v10_uses_v7_eval() {
  Board board;
  board.set_startpos();
  SearchConfig cfg{};
  cfg.depth = 1;
  cfg.version = EngineVersion::V10_TranspositionTable;

  const SearchResult result = search_best_move(board, cfg);
  return expect_true(result.has_move, "v10 returns a move from startpos");
}

bool test_v10_tt_reduces_nodes_on_repeat() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1")) {
    return expect_true(false, "test FEN parse");
  }

  SearchConfig cfg{};
  cfg.depth = 4;
  cfg.version = EngineVersion::V7_PhasedEval;
  const SearchResult v7 = search_best_move(board, cfg);

  cfg.version = EngineVersion::V10_TranspositionTable;
  const SearchResult v10 = search_best_move(board, cfg);

  return expect_true(v10.nodes <= v7.nodes, "v10 searches fewer or equal nodes than v7 at same depth");
}

bool test_tt_stores_hash_move() {
  search_common::TranspositionTable table(12);
  table.clear();

  const std::uint64_t key = 0xABCDEF0123456789ULL;
  const Move move(Square::E2, Square::E4, MoveFlag::DoublePush);
  table.store(key, 3, 25, search_common::TTBound::Exact, move, true);

  const Move stored = table.hash_move(key);
  return expect_true(moves_equal(stored, move), "TT returns stored hash move");
}

bool test_v11_fewer_nodes_than_v10() {
  Board board;
  if (!board.set_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1")) {
    return expect_true(false, "test FEN parse");
  }

  SearchConfig cfg{};
  cfg.depth = 5;
  cfg.version = EngineVersion::V10_TranspositionTable;
  const SearchResult v10 = search_best_move(board, cfg);

  cfg.version = EngineVersion::V11_TTHashMove;
  const SearchResult v11 = search_best_move(board, cfg);

  return expect_true(v11.nodes <= v10.nodes, "v11 searches fewer or equal nodes than v10");
}

bool test_v11_timed_search_returns_move() {
  Board board;
  board.set_startpos();
  SearchConfig cfg{};
  cfg.depth = 4;
  cfg.movetime_ms = 200;
  cfg.version = EngineVersion::V11_TTHashMove;

  const SearchResult result = search_best_move(board, cfg);
  return expect_true(result.has_move, "v11 timed search returns a move");
}

bool test_v12_search_completes() {
  Board board;
  board.set_startpos();
  SearchConfig cfg{};
  cfg.depth = 3;
  cfg.version = EngineVersion::V12_CheckExtension;

  const SearchResult result = search_best_move(board, cfg);
  return expect_true(result.has_move, "v12 returns a move from startpos");
}

bool test_v12_finds_mate_in_one() {
  Board board;
  if (!board.set_fen("6k1/5Q2/6K1/8/8/8/8/8 w - - 0 1")) {
    return expect_true(false, "mate-in-1 FEN parse");
  }

  SearchConfig cfg{};
  cfg.depth = 2;
  cfg.version = EngineVersion::V12_CheckExtension;
  const SearchResult result = search_best_move(board, cfg);
  if (!expect_true(result.has_move, "v12 finds a move in mate-in-1")) {
    return false;
  }
  return expect_true(result.score > 90000, "v12 scores mate in mate-in-1");
}

bool test_v12_timed_search_returns_move() {
  Board board;
  board.set_startpos();
  SearchConfig cfg{};
  cfg.depth = 4;
  cfg.movetime_ms = 200;
  cfg.version = EngineVersion::V12_CheckExtension;

  const SearchResult result = search_best_move(board, cfg);
  return expect_true(result.has_move, "v12 timed search returns a move");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();
  engine::init_zobrist();

  bool ok = true;
  ok = engine::test_zobrist_stable_and_move_changes_hash() && ok;
  ok = engine::test_tt_probe_store() && ok;
  ok = engine::test_v10_uses_v7_eval() && ok;
  ok = engine::test_v10_tt_reduces_nodes_on_repeat() && ok;
  ok = engine::test_tt_stores_hash_move() && ok;
  ok = engine::test_v11_fewer_nodes_than_v10() && ok;
  ok = engine::test_v11_timed_search_returns_move() && ok;
  ok = engine::test_v12_search_completes() && ok;
  ok = engine::test_v12_finds_mate_in_one() && ok;
  ok = engine::test_v12_timed_search_returns_move() && ok;

  if (ok) {
    std::cout << "tt_tests: all passed\n";
    return 0;
  }
  std::cerr << "tt_tests: failed\n";
  return 1;
}
