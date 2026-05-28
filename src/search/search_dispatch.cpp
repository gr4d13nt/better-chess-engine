#include "engine/search.hpp"

#include "search_common.hpp"

#include <algorithm>

namespace engine {

int evaluate(const Board& board) {
  return search_common::evaluate_improved(board);
}

SearchResult search_best_move(Board& board, const SearchConfig& cfg) {
  using namespace search_common;

  MoveList moves;
  generate_legal_moves(board, moves);
  if (moves.count == 0) {
    SearchResult result{};
    result.score = board.in_check(board.side_to_move()) ? -kMateScore : kDrawScore;
    result.nodes = 1;
    return result;
  }

  auto run_depth = [&](int depth, SearchState& st, SearchResult& out) -> bool {
    out = SearchResult{};
    out.score = -kMateScore;
    int root_alpha = -kMateScore;
    const int root_beta = kMateScore;

    for (int i = 0; i < moves.count; ++i) {
      const Move move = moves.moves[i];
      Undo undo;
      board.make_move(move, undo);
      int child_score = 0;
      bool ok = false;
      if (cfg.version == EngineVersion::V1_NoPruning) {
        ok = negamax_v1_no_pruning(board, depth - 1, st, child_score);
      } else if (cfg.version == EngineVersion::V5_MoveOrdering) {
        ok = negamax_v5_move_ordering(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V4_Quiescence) {
        ok = negamax_v4_quiescence(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else {
        ok = negamax_v2_alpha_beta(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      }
      board.unmake_move(move, undo);
      if (!ok) {
        return false;
      }

      const int score = -child_score;
      if (!out.has_move || score > out.score) {
        out.has_move = true;
        out.best_move = move;
        out.score = score;
      }
      root_alpha = std::max(root_alpha, score);
    }
    out.nodes = st.nodes;
    return true;
  };

  const EngineVersion eval_profile =
      (cfg.version == EngineVersion::V3_ImprovedEval || cfg.version == EngineVersion::V4_Quiescence ||
       cfg.version == EngineVersion::V5_MoveOrdering)
          ? EngineVersion::V3_ImprovedEval
          : EngineVersion::V1_NoPruning;

  if (cfg.movetime_ms <= 0) {
    SearchState st{};
    st.eval_profile = eval_profile;
    SearchResult result{};
    const int depth = std::max(1, cfg.depth);
    const bool ok = run_depth(depth, st, result);
    result.nodes = st.nodes;
    if (!ok || !result.has_move) {
      result.has_move = true;
      result.best_move = moves.moves[0];
      result.score = evaluate_for_side_to_move(board, eval_profile);
    }
    return result;
  }

  SearchState st{};
  st.use_time = true;
  st.eval_profile = eval_profile;
  st.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(cfg.movetime_ms);

  SearchResult best_complete{};
  const int max_depth = cfg.depth > 0 ? cfg.depth : kMaxSearchDepth;
  for (int d = 1; d <= max_depth; ++d) {
    SearchResult current{};
    if (!run_depth(d, st, current)) {
      break;
    }
    best_complete = current;
  }

  best_complete.nodes = st.nodes;
  if (!best_complete.has_move) {
    best_complete.has_move = true;
    best_complete.best_move = moves.moves[0];
    best_complete.score = evaluate_for_side_to_move(board, eval_profile);
  }
  return best_complete;
}

}  // namespace engine
