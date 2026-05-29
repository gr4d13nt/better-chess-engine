#include "engine/search.hpp"

#include "search_common.hpp"
#include "search_tt.hpp"

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

  const bool use_tt = cfg.version == EngineVersion::V10_TranspositionTable ||
                      cfg.version == EngineVersion::V11_TTHashMove ||
                      cfg.version == EngineVersion::V12_CheckExtension ||
                      cfg.version == EngineVersion::V13_MopUpEval;
  const bool use_root_pv = cfg.version == EngineVersion::V11_TTHashMove ||
                           cfg.version == EngineVersion::V12_CheckExtension ||
                           cfg.version == EngineVersion::V13_MopUpEval;
  if (use_tt) {
    tt_clear();
  }

  auto run_depth = [&](int depth, int root_alpha, int root_beta, SearchState& st,
                       SearchResult& out) -> bool {
    out = SearchResult{};
    out.score = -kMateScore;

    for (int i = 0; i < moves.count; ++i) {
      const Move move = moves.moves[i];
      Undo undo;
      board.make_move(move, undo);
      int child_score = 0;
      bool ok = false;
      if (cfg.version == EngineVersion::V1_NoPruning) {
        ok = negamax_v1_no_pruning(board, depth - 1, st, child_score);
      } else if (cfg.version == EngineVersion::V13_MopUpEval ||
                 cfg.version == EngineVersion::V12_CheckExtension) {
        ok = negamax_v12_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V11_TTHashMove) {
        ok = negamax_v11_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V10_TranspositionTable) {
        ok = negamax_v10_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V5_MoveOrdering ||
                 cfg.version == EngineVersion::V6_PawnKingEval ||
                 cfg.version == EngineVersion::V7_PhasedEval ||
                 cfg.version == EngineVersion::V8_KnightOutposts ||
                 cfg.version == EngineVersion::V9_RookPlacement) {
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

  EngineVersion eval_profile = EngineVersion::V1_NoPruning;
  if (cfg.version == EngineVersion::V13_MopUpEval) {
    eval_profile = EngineVersion::V13_MopUpEval;
  } else if (cfg.version == EngineVersion::V12_CheckExtension ||
      cfg.version == EngineVersion::V11_TTHashMove ||
      cfg.version == EngineVersion::V10_TranspositionTable) {
    eval_profile = EngineVersion::V7_PhasedEval;
  } else if (cfg.version == EngineVersion::V9_RookPlacement) {
    eval_profile = EngineVersion::V9_RookPlacement;
  } else if (cfg.version == EngineVersion::V8_KnightOutposts) {
    eval_profile = EngineVersion::V8_KnightOutposts;
  } else if (cfg.version == EngineVersion::V7_PhasedEval) {
    eval_profile = EngineVersion::V7_PhasedEval;
  } else if (cfg.version == EngineVersion::V6_PawnKingEval) {
    eval_profile = EngineVersion::V6_PawnKingEval;
  } else if (cfg.version == EngineVersion::V3_ImprovedEval ||
             cfg.version == EngineVersion::V4_Quiescence ||
             cfg.version == EngineVersion::V5_MoveOrdering) {
    eval_profile = EngineVersion::V3_ImprovedEval;
  }

  auto ensure_fallback = [&](SearchResult& result, const SearchState& st) {
    result.nodes = st.nodes;
    if (!result.has_move) {
      result.has_move = true;
      result.best_move = moves.moves[0];
      result.score = evaluate_for_side_to_move(board, eval_profile);
    }
  };

  if (cfg.movetime_ms <= 0) {
    SearchState st{};
    st.eval_profile = eval_profile;
    if (use_tt) {
      st.tt = &global_tt();
    }
    SearchResult result{};
    const int depth = std::max(1, cfg.depth);
    const bool ok = run_depth(depth, -kMateScore, kMateScore, st, result);
    if (!ok) {
      result = SearchResult{};
    }
    ensure_fallback(result, st);
    return result;
  }

  SearchState st{};
  st.use_time = true;
  st.eval_profile = eval_profile;
  if (use_tt) {
    st.tt = &global_tt();
  }
  st.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(cfg.movetime_ms);

  SearchResult best_complete{};
  Move pv_move{};
  const int max_depth = cfg.depth > 0 ? cfg.depth : kMaxSearchDepth;
  for (int d = 1; d <= max_depth; ++d) {
    if (use_root_pv) {
      prioritize_move(moves, pv_move);
    }
    SearchResult current{};
    if (!run_depth(d, -kMateScore, kMateScore, st, current)) {
      break;
    }
    best_complete = current;
    if (current.has_move) {
      pv_move = current.best_move;
    }
  }

  ensure_fallback(best_complete, st);
  return best_complete;
}

}  // namespace engine
