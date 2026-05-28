#pragma once

#include "engine/attacks.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"

#include <chrono>

namespace engine::search_common {

constexpr int kMateScore = 100000;
constexpr int kDrawScore = 0;
constexpr int kMaxSearchDepth = 64;

struct SearchState {
  int nodes = 0;
  bool use_time = false;
  std::chrono::steady_clock::time_point deadline{};
  bool stopped = false;
  EngineVersion eval_profile = EngineVersion::V1_NoPruning;
};

bool should_stop(SearchState& st);

int evaluate_material_only(const Board& board);
int evaluate_improved(const Board& board);
int evaluate_for_side_to_move(const Board& board, EngineVersion eval_profile);

// Version-specific recursive search kernels.
bool negamax_v1_no_pruning(Board& board, int depth, SearchState& st, int& out_score);
bool negamax_v2_alpha_beta(Board& board, int depth, int alpha, int beta, SearchState& st,
                           int& out_score);
bool negamax_v4_quiescence(Board& board, int depth, int alpha, int beta, SearchState& st,
                           int& out_score);
bool negamax_v5_move_ordering(Board& board, int depth, int alpha, int beta, SearchState& st,
                              int& out_score);

}  // namespace engine::search_common
