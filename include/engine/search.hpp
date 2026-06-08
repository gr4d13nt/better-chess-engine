#pragma once

#include "engine/board.hpp"
#include "engine/move.hpp"

#include <atomic>
#include <cstdint>
#include <vector>

namespace engine {

namespace search_common {
class TranspositionTable;
}

enum class EngineVersion : int {
  V1_NoPruning = 1,
  V2_AlphaBeta = 2,
  V3_ImprovedEval = 3,
  V4_Quiescence = 4,
  V5_MoveOrdering = 5,
  V6_PawnKingEval = 6,
  V7_PhasedEval = 7,
  V8_KnightOutposts = 8,
  V9_RookPlacement = 9,
  V10_TranspositionTable = 10,
  V11_TTHashMove = 11,
  V12_CheckExtension = 12,
  V13_MopUpEval = 13,
  V14_RepetitionDraw = 14,
  V15_PiecePlacement = 15,
  V16_SeeQsearch = 16,
  V17_DeltaQsearch = 17,
  V18_NullMove = 18,
  V19_KillerHistory = 19,
  V20_PersistentTT = 20,
  V21_PassedPawns = 21,
  V22_ExtendedPawnStructure = 22,
  V23_Space = 23,
  V24_HangingPieces = 24,
  V25_OpeningBook = 25,
  V26_LMR = 26,
  V27_PVS = 27,
  V28_FutilityLmp = 28,
  V29_LazySMP = 29,
  V30_TimeManagement = 30,
};

inline bool version_uses_opening_book(EngineVersion v) {
  return v == EngineVersion::V25_OpeningBook || v == EngineVersion::V26_LMR ||
         v == EngineVersion::V27_PVS || v == EngineVersion::V28_FutilityLmp ||
         v == EngineVersion::V29_LazySMP || v == EngineVersion::V30_TimeManagement;
}

struct SearchConfig {
  int depth = 4;
  EngineVersion version = EngineVersion::V1_NoPruning;
  // Per-move search budget in milliseconds. 0 means depth-only.
  int movetime_ms = 0;
  // v29 lazy SMP: worker count. 0 means hardware_concurrency() when movetime_ms > 0.
  int num_threads = 0;
  // Game position hashes from the start through the current root (for v14 repetition).
  std::vector<std::uint64_t> repetition_history{};
  // v20/v21: when true, clears the global TT before this search (new game / loaded FEN).
  bool clear_transposition_table = false;
  // v25–v30 only: when true, play from the Polyglot book before searching.
  bool use_opening_book = false;
  // Lazy SMP internals (set by search_dispatch / lazy SMP wrapper).
  search_common::TranspositionTable* local_tt = nullptr;
  std::atomic<bool>* shared_stop = nullptr;
  // Lazy SMP: add to iterative-deepening depth (0 = same as main, 1 = +1 ply).
  int smp_depth_offset = 0;
};

inline bool opening_book_enabled(const SearchConfig& cfg) {
  return version_uses_opening_book(cfg.version) && cfg.use_opening_book;
}

struct SearchResult {
  Move best_move{};
  int score = 0;
  int nodes = 0;
  int depth_reached = 0;
  bool has_move = false;
};

// Versioned search entrypoint.
SearchResult search_best_move(Board& board, const SearchConfig& cfg);

// Clears the global transposition table (used by v20 on new game).
void clear_transposition_table();

// Static evaluation from White perspective.
int evaluate(const Board& board);

}  // namespace engine
