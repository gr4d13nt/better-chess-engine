#pragma once

#include "engine/board.hpp"
#include "engine/move.hpp"

namespace engine {

enum class EngineVersion : int {
  V1_NoPruning = 1,
  V2_AlphaBeta = 2,
};

struct SearchConfig {
  int depth = 4;
  EngineVersion version = EngineVersion::V1_NoPruning;
  // Per-move search budget in milliseconds. 0 means depth-only.
  int movetime_ms = 0;
};

struct SearchResult {
  Move best_move{};
  int score = 0;
  int nodes = 0;
  bool has_move = false;
};

// Versioned search entrypoint.
SearchResult search_best_move(Board& board, const SearchConfig& cfg);

// Static evaluation from White perspective.
int evaluate(const Board& board);

}  // namespace engine
