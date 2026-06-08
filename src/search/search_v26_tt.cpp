#include "search_common.hpp"

#include "engine/bitboard.hpp"
#include "engine/see.hpp"
#include "search_tt.hpp"

#include <algorithm>
#include <vector>

// v26 = v25 (v24 eval + book + persistent TT + root PV) with late-move reductions.
// Search kernel is isolated in this file so v19–v25 stay on the unchanged v19 path.

namespace engine::search_common {
namespace {

constexpr int kHashMoveScore = 2'000'000;
constexpr int kCheckMoveScore = 950'000;
constexpr int kKiller0Score = 900'000;
constexpr int kKiller1Score = 800'000;
constexpr int kMaxCheckExtensions = 1;

constexpr int kDeltaMargin = 150;
constexpr int kSeePruneThreshold = -100;

constexpr int kNullMinDepth = 3;
constexpr int kNullMinNonPawns = 2;

constexpr int kHistoryMax = 8192;

constexpr int kLmrMinDepth = 3;
constexpr int kLmrMinMoveIndex = 4;

constexpr int kPieceValues[kNumPieceTypes] = {
    100, 320, 330, 500, 900, 0,
};

int lmr_reduction(int depth, int move_index) {
  if (move_index < kLmrMinMoveIndex) {
    return 0;
  }
  int r = 1;
  if (depth >= 6 && move_index >= 10) {
    r = 2;
  }
  if (depth >= 9 && move_index >= 20) {
    r = 3;
  }
  return std::min(r, depth - 2);
}

bool is_repeated_position(const SearchState& st, std::uint64_t key) {
  for (std::uint64_t seen : st.repetition) {
    if (seen == key) {
      return true;
    }
  }
  return false;
}

struct RepetitionPop {
  SearchState& st;
  ~RepetitionPop() { st.repetition.pop_back(); }
};

bool is_tactical_move(const Move& m) {
  return m.flag == MoveFlag::Capture || m.flag == MoveFlag::PromotionCapture ||
         m.flag == MoveFlag::EnPassant || m.flag == MoveFlag::Promotion;
}

bool is_qsearch_capture(const Move& m) {
  return m.flag == MoveFlag::Capture || m.flag == MoveFlag::PromotionCapture ||
         m.flag == MoveFlag::EnPassant;
}

int count_non_pawns(const Board& board, Color c) {
  int n = 0;
  for (int pt = static_cast<int>(PieceType::Knight); pt <= static_cast<int>(PieceType::Queen); ++pt) {
    n += bb::popcount(board.pieces(c, static_cast<PieceType>(pt)));
  }
  return n;
}

bool has_null_move_material(const Board& board) {
  return count_non_pawns(board, board.side_to_move()) >= kNullMinNonPawns;
}

bool gives_check(const Board& board, const Move& move) {
  Board copy = board;
  Undo undo;
  copy.make_move(move, undo);
  return copy.in_check(!board.side_to_move());
}

bool has_checking_move(const Board& board) {
  MoveList list;
  generate_legal_moves(board, list);
  for (int i = 0; i < list.count; ++i) {
    if (gives_check(board, list.moves[i])) {
      return true;
    }
  }
  return false;
}

bool opponent_has_checking_move(const Board& board) {
  Board copy = board;
  NullUndo undo;
  copy.make_null_move(undo);
  return has_checking_move(copy);
}

int null_reduction(int depth) {
  return depth >= 6 ? 3 : 2;
}

void update_killers(SearchState& st, int ply, const Move& move) {
  if (ply < 0 || ply >= kMaxSearchDepth) {
    return;
  }
  if (moves_equal(move, st.killers[ply][0])) {
    return;
  }
  st.killers[ply][1] = st.killers[ply][0];
  st.killers[ply][0] = move;
}

void update_history(SearchState& st, Color side, const Move& move, int depth) {
  if (is_tactical_move(move)) {
    return;
  }
  int& entry = st.history[static_cast<int>(side)][square_index(move.from)][square_index(move.to)];
  entry += depth * depth;
  if (entry > kHistoryMax) {
    entry = kHistoryMax;
  }
}

int captured_piece_value(const Board& board, const Move& m) {
  if (m.flag == MoveFlag::EnPassant) {
    return kPieceValues[static_cast<int>(PieceType::Pawn)];
  }
  if (m.flag == MoveFlag::Capture || m.flag == MoveFlag::PromotionCapture) {
    const Piece captured = board.piece_on(m.to);
    const PieceType cap_type = captured == Piece::None ? PieceType::Pawn : piece_type(captured);
    return kPieceValues[static_cast<int>(cap_type)];
  }
  return 0;
}

bool prune_losing_qcapture(const Board& board, const Move& m) {
  if (m.flag == MoveFlag::PromotionCapture) {
    return false;
  }
  if (!is_qsearch_capture(m)) {
    return false;
  }
  return see_capture(board, m) <= kSeePruneThreshold;
}

bool prune_delta_qcapture(int stand_pat, int alpha, const Board& board, const Move& m) {
  if (m.flag == MoveFlag::Promotion || m.flag == MoveFlag::PromotionCapture) {
    return false;
  }
  if (!is_qsearch_capture(m)) {
    return false;
  }
  return stand_pat + captured_piece_value(board, m) + kDeltaMargin < alpha;
}

int move_score(const Board& board, const Move& m, const Move* hash_move, const SearchState& st,
               int ply, Color side) {
  int score = 0;
  if (hash_move != nullptr && moves_equal(m, *hash_move)) {
    score += kHashMoveScore;
  }

  const Piece moving_piece = board.piece_on(m.from);
  const PieceType moving_type = piece_type(moving_piece);

  if (m.flag == MoveFlag::Capture || m.flag == MoveFlag::PromotionCapture) {
    const Piece captured = board.piece_on(m.to);
    const PieceType cap_type = captured == Piece::None ? PieceType::Pawn : piece_type(captured);
    score += 100000 + (10 * kPieceValues[static_cast<int>(cap_type)]) -
             kPieceValues[static_cast<int>(moving_type)];
  } else if (m.flag == MoveFlag::EnPassant) {
    score += 100000 + (10 * kPieceValues[static_cast<int>(PieceType::Pawn)]) -
             kPieceValues[static_cast<int>(moving_type)];
  } else {
    if (gives_check(board, m)) {
      score += kCheckMoveScore;
    }
    if (ply >= 0 && ply < kMaxSearchDepth) {
      if (moves_equal(m, st.killers[ply][0])) {
        score += kKiller0Score;
      } else if (moves_equal(m, st.killers[ply][1])) {
        score += kKiller1Score;
      }
    }
    score += st.history[static_cast<int>(side)][square_index(m.from)][square_index(m.to)];
  }

  if (m.flag == MoveFlag::Promotion || m.flag == MoveFlag::PromotionCapture) {
    score += 50000 + kPieceValues[static_cast<int>(m.promotion)];
  }

  score += square_index(m.to) - square_index(m.from);
  return score;
}

std::vector<Move> ordered_moves(const Board& board, const MoveList& moves, bool tactical_only,
                                const Move* hash_move, const SearchState& st, int ply) {
  const Color side = board.side_to_move();
  std::vector<std::pair<int, Move>> scored;
  scored.reserve(moves.count);
  for (int i = 0; i < moves.count; ++i) {
    const Move m = moves.moves[i];
    if (tactical_only && !is_tactical_move(m)) {
      continue;
    }
    scored.push_back({move_score(board, m, hash_move, st, ply, side), m});
  }
  std::sort(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  std::vector<Move> out;
  out.reserve(scored.size());
  for (const auto& [_, m] : scored) {
    out.push_back(m);
  }
  return out;
}

bool quiescence_v26(Board& board, int alpha, int beta, SearchState& st, int qdepth, int ply,
                    int& out_score);

bool negamax_v26_tt_impl(Board& board, int depth, int alpha, int beta, SearchState& st, int ply,
                         int extensions, bool allow_null, int& out_score);

bool quiescence_v26(Board& board, int alpha, int beta, SearchState& st, int qdepth, int ply,
                    int& out_score) {
  if (should_stop(st)) {
    return false;
  }
  st.nodes++;

  MoveList legal;
  generate_legal_moves(board, legal);
  if (legal.count == 0) {
    if (board.in_check(board.side_to_move())) {
      out_score = -kMateScore;
      return true;
    }
    out_score = kDrawScore;
    return true;
  }

  const bool in_check = board.in_check(board.side_to_move());
  const int stand_pat = evaluate_for_side_to_move(board, st.eval_profile);

  if (!in_check) {
    if (stand_pat >= beta) {
      out_score = stand_pat;
      return true;
    }
    alpha = std::max(alpha, stand_pat);
  }

  const Move hash_move = st.tt != nullptr ? st.tt->hash_move(board.zobrist()) : Move{};
  const Move* hash_ptr = hash_move.from != Square::None ? &hash_move : nullptr;
  const std::vector<Move> moves = ordered_moves(board, legal, !in_check, hash_ptr, st, ply);
  bool searched_any = false;
  for (const Move& move : moves) {
    if (!in_check && qdepth > 0) {
      if (prune_delta_qcapture(stand_pat, alpha, board, move)) {
        continue;
      }
      if (prune_losing_qcapture(board, move)) {
        continue;
      }
    }

    searched_any = true;
    Undo undo;
    board.make_move(move, undo);
    int child_score = 0;
    if (!quiescence_v26(board, -beta, -alpha, st, qdepth + 1, ply + 1, child_score)) {
      board.unmake_move(move, undo);
      return false;
    }
    board.unmake_move(move, undo);

    const int score = -child_score;
    if (score >= beta) {
      out_score = score;
      return true;
    }
    alpha = std::max(alpha, score);
  }

  out_score = searched_any ? alpha : stand_pat;
  return true;
}

bool negamax_v26_tt_impl(Board& board, int depth, int alpha, int beta, SearchState& st, int ply,
                         int extensions, bool allow_null, int& out_score) {
  if (should_stop(st)) {
    return false;
  }
  st.nodes++;

  const std::uint64_t key = board.zobrist();
  if (is_repeated_position(st, key)) {
    out_score = kDrawScore;
    return true;
  }
  st.repetition.push_back(key);
  RepetitionPop pop{st};

  int tt_score = 0;
  if (depth > 0 && st.tt != nullptr &&
      st.tt->probe(key, depth, alpha, beta, st.tt_generation, tt_score)) {
    out_score = tt_score;
    return true;
  }

  MoveList legal;
  generate_legal_moves(board, legal);

  if (legal.count == 0) {
    if (board.in_check(board.side_to_move())) {
      out_score = -kMateScore + (8 - depth);
      return true;
    }
    out_score = kDrawScore;
    return true;
  }

  if (depth == 0) {
    return quiescence_v26(board, alpha, beta, st, 0, ply, out_score);
  }

  if (allow_null && depth >= kNullMinDepth && extensions == 0 &&
      !board.in_check(board.side_to_move()) && has_null_move_material(board) &&
      !has_checking_move(board) && !opponent_has_checking_move(board)) {
    const int R = null_reduction(depth);
    const int null_depth = depth - R;
    if (null_depth >= 1) {
      NullUndo null_undo;
      board.make_null_move(null_undo);
      int null_score = 0;
      const bool ok =
          negamax_v26_tt_impl(board, null_depth, -beta, -beta + 1, st, ply + 1, 0, false, null_score);
      board.unmake_null_move(null_undo);
      if (ok && -null_score >= beta) {
        out_score = beta;
        return true;
      }
    }
  }

  const int alpha_orig = alpha;
  const Color side = board.side_to_move();
  const Move hash_move = st.tt != nullptr ? st.tt->hash_move(key) : Move{};
  const Move* hash_ptr = hash_move.from != Square::None ? &hash_move : nullptr;
  const std::vector<Move> moves = ordered_moves(board, legal, false, hash_ptr, st, ply);

  int best = -kMateScore;
  Move best_move{};
  bool have_best_move = false;
  bool cutoff = false;
  Move cutoff_move{};

  const bool in_check = board.in_check(board.side_to_move());
  int move_index = 0;
  for (const Move& move : moves) {
    Undo undo;
    board.make_move(move, undo);

    int child_depth = depth - 1;
    int child_extensions = extensions;
    const bool move_gives_check = board.in_check(board.side_to_move());
    if (move_gives_check && extensions < kMaxCheckExtensions) {
      child_depth = depth;
      child_extensions = extensions + 1;
    }

    int reduction = 0;
    if (move_index > 0 && !in_check && child_extensions == 0 && depth >= kLmrMinDepth &&
        !is_tactical_move(move) && !move_gives_check) {
      reduction = lmr_reduction(depth, move_index);
    }
    int search_depth = child_depth - reduction;
    if (search_depth < 0) {
      search_depth = 0;
    }

    int child_score = 0;
    if (!negamax_v26_tt_impl(board, search_depth, -beta, -alpha, st, ply + 1, child_extensions,
                             true, child_score)) {
      board.unmake_move(move, undo);
      return false;
    }

    int score = -child_score;
    if (reduction > 0 && score > alpha && score < beta) {
      if (!negamax_v26_tt_impl(board, child_depth, -beta, -alpha, st, ply + 1, child_extensions,
                               true, child_score)) {
        board.unmake_move(move, undo);
        return false;
      }
      score = -child_score;
    }
    board.unmake_move(move, undo);

    if (score > best) {
      best = score;
      best_move = move;
      have_best_move = true;
    }
    alpha = std::max(alpha, score);
    if (alpha >= beta) {
      cutoff = true;
      cutoff_move = move;
      if (!is_tactical_move(move)) {
        update_killers(st, ply, move);
        update_history(st, side, move, depth);
      }
      break;
    }
    ++move_index;
  }

  if (st.tt != nullptr) {
    TTBound bound = TTBound::Exact;
    if (best <= alpha_orig) {
      bound = TTBound::Upper;
    } else if (cutoff) {
      bound = TTBound::Lower;
    }

    Move move_to_store{};
    bool store_move = false;
    if (cutoff) {
      move_to_store = cutoff_move;
      store_move = true;
    } else if (have_best_move && bound == TTBound::Exact) {
      move_to_store = best_move;
      store_move = true;
    }
    st.tt->store(key, depth, best, bound, st.tt_generation, move_to_store, store_move);
  }

  out_score = best;
  return true;
}

}  // namespace

bool negamax_v26_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score) {
  return negamax_v26_tt_impl(board, depth, alpha, beta, st, 0, 0, true, out_score);
}

}  // namespace engine::search_common
