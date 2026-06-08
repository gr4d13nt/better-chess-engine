#include "engine/search.hpp"

#include "engine/book.hpp"

#include "search_common.hpp"
#include "search_internal.hpp"
#include "search_tt.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace engine {

namespace {

bool version_uses_v28_kernel(EngineVersion v) {
  return v == EngineVersion::V28_FutilityLmp || v == EngineVersion::V29_LazySMP ||
         v == EngineVersion::V30_TimeManagement;
}

bool version_uses_v27_pvs_root(EngineVersion v) {
  return v == EngineVersion::V27_PVS || version_uses_v28_kernel(v);
}

}  // namespace

void clear_transposition_table() { search_common::tt_clear(); }

int evaluate(const Board& board) {
  return search_common::evaluate_improved(board);
}

SearchResult search_best_move_single(Board& board, const SearchConfig& cfg) {
  using namespace search_common;

  MoveList moves;
  generate_legal_moves(board, moves);
  if (moves.count == 0) {
    SearchResult result{};
    result.score = board.in_check(board.side_to_move()) ? -kMateScore : kDrawScore;
    result.nodes = 1;
    return result;
  }

  const bool use_book = opening_book_enabled(cfg);
  if (use_book) {
    if (const auto book_move = probe_opening_book(board)) {
      SearchResult result{};
      result.best_move = *book_move;
      result.has_move = true;
      result.nodes = 0;
      return result;
    }
  }

  const bool use_tt = cfg.version == EngineVersion::V10_TranspositionTable ||
                      cfg.version == EngineVersion::V11_TTHashMove ||
                      cfg.version == EngineVersion::V12_CheckExtension ||
                      cfg.version == EngineVersion::V13_MopUpEval ||
                      cfg.version == EngineVersion::V14_RepetitionDraw ||
                      cfg.version == EngineVersion::V15_PiecePlacement ||
                      cfg.version == EngineVersion::V16_SeeQsearch ||
                      cfg.version == EngineVersion::V17_DeltaQsearch ||
                      cfg.version == EngineVersion::V18_NullMove ||
                      cfg.version == EngineVersion::V19_KillerHistory ||
                      cfg.version == EngineVersion::V20_PersistentTT ||
                      cfg.version == EngineVersion::V21_PassedPawns ||
                      cfg.version == EngineVersion::V22_ExtendedPawnStructure ||
                      cfg.version == EngineVersion::V23_Space ||
                      cfg.version == EngineVersion::V24_HangingPieces ||
                      cfg.version == EngineVersion::V25_OpeningBook ||
                      cfg.version == EngineVersion::V26_LMR ||
                      cfg.version == EngineVersion::V27_PVS ||
                      version_uses_v28_kernel(cfg.version);
  const bool use_root_pv = cfg.version == EngineVersion::V11_TTHashMove ||
                           cfg.version == EngineVersion::V12_CheckExtension ||
                           cfg.version == EngineVersion::V13_MopUpEval ||
                           cfg.version == EngineVersion::V14_RepetitionDraw ||
                           cfg.version == EngineVersion::V15_PiecePlacement ||
                           cfg.version == EngineVersion::V16_SeeQsearch ||
                           cfg.version == EngineVersion::V17_DeltaQsearch ||
                           cfg.version == EngineVersion::V18_NullMove ||
                           cfg.version == EngineVersion::V19_KillerHistory ||
                           cfg.version == EngineVersion::V20_PersistentTT ||
                           cfg.version == EngineVersion::V21_PassedPawns ||
                           cfg.version == EngineVersion::V22_ExtendedPawnStructure ||
                           cfg.version == EngineVersion::V23_Space ||
                           cfg.version == EngineVersion::V24_HangingPieces ||
                           cfg.version == EngineVersion::V25_OpeningBook ||
                           cfg.version == EngineVersion::V26_LMR ||
                           cfg.version == EngineVersion::V27_PVS ||
                           version_uses_v28_kernel(cfg.version);
  const bool persistent_tt = cfg.version == EngineVersion::V20_PersistentTT ||
                             cfg.version == EngineVersion::V21_PassedPawns ||
                             cfg.version == EngineVersion::V22_ExtendedPawnStructure ||
                             cfg.version == EngineVersion::V23_Space ||
                             cfg.version == EngineVersion::V24_HangingPieces ||
                             cfg.version == EngineVersion::V25_OpeningBook ||
                             cfg.version == EngineVersion::V26_LMR ||
                             cfg.version == EngineVersion::V27_PVS ||
                             version_uses_v28_kernel(cfg.version);
  if (use_tt && cfg.local_tt == nullptr) {
    if (!persistent_tt || cfg.clear_transposition_table) {
      tt_clear();
    }
  }

  auto run_depth = [&](int depth, int root_alpha, int root_beta, SearchState& st,
                       SearchResult& out) -> bool {
    out = SearchResult{};
    out.score = -kMateScore;

    for (int i = 0; i < moves.count; ++i) {
      std::memset(st.killers, 0, sizeof(st.killers));
      std::memset(st.history, 0, sizeof(st.history));
      st.repetition = cfg.repetition_history;
      if (st.tt != nullptr) {
        st.tt_generation = st.tt->new_search();
      }

      const Move move = moves.moves[i];
      Undo undo;
      board.make_move(move, undo);
      int child_score = 0;
      bool ok = false;
      if (cfg.version == EngineVersion::V1_NoPruning) {
        ok = negamax_v1_no_pruning(board, depth - 1, st, child_score);
      } else if (version_uses_v27_pvs_root(cfg.version)) {
        const auto run_child = [&](int ca, int cb) -> bool {
          if (version_uses_v28_kernel(cfg.version)) {
            return negamax_v28_tt(board, depth - 1, ca, cb, st, child_score);
          }
          return negamax_v27_tt(board, depth - 1, ca, cb, st, child_score);
        };
        if (i == 0) {
          ok = run_child(-root_beta, -root_alpha);
        } else {
          ok = run_child(-(root_alpha + 1), -root_alpha);
          if (ok) {
            const int scout_score = -child_score;
            if (scout_score > root_alpha && scout_score < root_beta) {
              ok = run_child(-root_beta, -root_alpha);
            }
          }
        }
      } else if (cfg.version == EngineVersion::V26_LMR) {
        ok = negamax_v26_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V24_HangingPieces ||
                 cfg.version == EngineVersion::V25_OpeningBook) {
        ok = negamax_v24_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V23_Space) {
        ok = negamax_v23_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V22_ExtendedPawnStructure) {
        ok = negamax_v22_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V21_PassedPawns) {
        ok = negamax_v21_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V20_PersistentTT) {
        ok = negamax_v20_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V19_KillerHistory) {
        ok = negamax_v19_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V18_NullMove) {
        ok = negamax_v18_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V17_DeltaQsearch) {
        ok = negamax_v17_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V16_SeeQsearch) {
        ok = negamax_v16_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else if (cfg.version == EngineVersion::V14_RepetitionDraw ||
                 cfg.version == EngineVersion::V15_PiecePlacement) {
        ok = negamax_v14_tt(board, depth - 1, -root_beta, -root_alpha, st, child_score);
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
  if (cfg.version == EngineVersion::V24_HangingPieces ||
      cfg.version == EngineVersion::V25_OpeningBook ||
      cfg.version == EngineVersion::V26_LMR ||
      cfg.version == EngineVersion::V27_PVS ||
      version_uses_v28_kernel(cfg.version) ||
      cfg.version == EngineVersion::V23_Space ||
      cfg.version == EngineVersion::V22_ExtendedPawnStructure ||
      cfg.version == EngineVersion::V21_PassedPawns ||
      cfg.version == EngineVersion::V20_PersistentTT ||
      cfg.version == EngineVersion::V19_KillerHistory ||
      cfg.version == EngineVersion::V18_NullMove ||
      cfg.version == EngineVersion::V17_DeltaQsearch ||
      cfg.version == EngineVersion::V16_SeeQsearch ||
      cfg.version == EngineVersion::V15_PiecePlacement) {
    if (cfg.version == EngineVersion::V24_HangingPieces ||
        cfg.version == EngineVersion::V25_OpeningBook ||
        cfg.version == EngineVersion::V26_LMR ||
        cfg.version == EngineVersion::V27_PVS ||
        version_uses_v28_kernel(cfg.version)) {
      eval_profile = EngineVersion::V24_HangingPieces;
    } else if (cfg.version == EngineVersion::V23_Space) {
      eval_profile = EngineVersion::V23_Space;
    } else if (cfg.version == EngineVersion::V22_ExtendedPawnStructure) {
      eval_profile = EngineVersion::V22_ExtendedPawnStructure;
    } else if (cfg.version == EngineVersion::V21_PassedPawns) {
      eval_profile = EngineVersion::V21_PassedPawns;
    } else {
      eval_profile = EngineVersion::V15_PiecePlacement;
    }
  } else if (cfg.version == EngineVersion::V14_RepetitionDraw ||
             cfg.version == EngineVersion::V13_MopUpEval) {
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

  auto init_search_state = [&](SearchState& st) {
    st.eval_profile = eval_profile;
    st.shared_stop = cfg.shared_stop;
    if (cfg.local_tt != nullptr) {
      st.tt = cfg.local_tt;
      st.tt_generation = st.tt->new_search();
    } else if (use_tt) {
      st.tt = &global_tt();
      st.tt_generation = st.tt->new_search();
    }
    st.repetition = cfg.repetition_history;
  };

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
    init_search_state(st);
    SearchResult result{};
    const int depth = std::max(1, cfg.depth);
    const bool ok = run_depth(depth, -kMateScore, kMateScore, st, result);
    if (!ok) {
      result = SearchResult{};
    }
    ensure_fallback(result, st);
    result.depth_reached = depth;
    return result;
  }

  SearchState st{};
  st.use_time = true;
  init_search_state(st);
  const bool use_time_management = cfg.version == EngineVersion::V30_TimeManagement;
  st.use_time_management = use_time_management;

  const auto search_start = std::chrono::steady_clock::now();
  const auto hard_deadline =
      search_start + std::chrono::milliseconds(cfg.movetime_ms);
  st.hard_deadline = hard_deadline;
  if (!use_time_management) {
    st.deadline = hard_deadline;
  }

  SearchResult best_complete{};
  Move pv_move{};
  const int max_depth = cfg.depth > 0 ? cfg.depth : kMaxSearchDepth;
  int last_depth_ms = 0;
  for (int d = 1; d <= max_depth; ++d) {
    const int search_depth = d + cfg.smp_depth_offset;
    if (search_depth > max_depth) {
      continue;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now >= hard_deadline) {
      break;
    }

    if (use_time_management && d > 1 && last_depth_ms > 0) {
      const auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    hard_deadline - now)
                                    .count();
      if (remaining_ms <= 0 || remaining_ms < (last_depth_ms * 135) / 100) {
        break;
      }
    }

    if (use_time_management) {
      const auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    hard_deadline - now)
                                    .count();
      const int slice_ms = static_cast<int>((remaining_ms * 88) / 100);
      st.deadline = now + std::chrono::milliseconds(std::max(slice_ms, 1));
    }

    st.stopped = false;
    const auto depth_start = std::chrono::steady_clock::now();
    if (use_root_pv) {
      prioritize_move(moves, pv_move);
    }

    SearchResult current{};
    if (!run_depth(search_depth, -kMateScore, kMateScore, st, current)) {
      current = SearchResult{};
      break;
    }

    if (!current.has_move) {
      if (d > 1) {
        break;
      }
      ensure_fallback(current, st);
    }

    best_complete = current;
    best_complete.depth_reached = search_depth;
    if (current.has_move) {
      pv_move = current.best_move;
    }

    if (use_time_management) {
      last_depth_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - depth_start)
                                           .count());
      if (last_depth_ms < 1) {
        last_depth_ms = 1;
      }
    }
  }

  ensure_fallback(best_complete, st);
  return best_complete;
}

}  // namespace engine
