#pragma once

#include "engine/attacks.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"

#include "search_tt.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

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
  TranspositionTable* tt = nullptr;
  // Game history + positions on the current search path (v14 repetition).
  std::vector<std::uint64_t> repetition{};
};

bool should_stop(SearchState& st);

int evaluate_material_only(const Board& board);
int evaluate_improved(const Board& board);
int evaluate_v6(const Board& board);
int evaluate_v7(const Board& board);
int evaluate_v8(const Board& board);
int evaluate_v9(const Board& board);
int evaluate_v13(const Board& board);
int evaluate_v15(const Board& board);
// MG/EG components of evaluate_improved (material, PST, mobility, bishop pair).
void evaluate_improved_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                             int& eg_black);
// Pawn/king structure nets (White - Black), phase-tapered; layered on evaluate_improved in v6.
int evaluate_pawn_structure_net(const Board& board);
int evaluate_king_safety_net(const Board& board);
int evaluate_for_side_to_move(const Board& board, EngineVersion eval_profile);

// Version-specific recursive search kernels.
bool negamax_v1_no_pruning(Board& board, int depth, SearchState& st, int& out_score);
bool negamax_v2_alpha_beta(Board& board, int depth, int alpha, int beta, SearchState& st,
                           int& out_score);
bool negamax_v4_quiescence(Board& board, int depth, int alpha, int beta, SearchState& st,
                           int& out_score);
bool negamax_v5_move_ordering(Board& board, int depth, int alpha, int beta, SearchState& st,
                              int& out_score);
bool negamax_v10_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score);
bool negamax_v11_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score);
bool negamax_v12_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score);
bool negamax_v14_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score);

bool negamax_v16_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score);

bool negamax_v17_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score);

bool negamax_v18_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score);

void prioritize_move(MoveList& moves, const Move& prefer);

}  // namespace engine::search_common
