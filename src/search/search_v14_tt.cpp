#include "search_common.hpp"

#include "search_tt.hpp"

#include <algorithm>
#include <vector>

namespace engine::search_common {
namespace {

constexpr int kHashMoveScore = 2'000'000;
constexpr int kMaxCheckExtensions = 1;

constexpr int kPieceValues[kNumPieceTypes] = {
    100,  // Pawn
    320,  // Knight
    330,  // Bishop
    500,  // Rook
    900,  // Queen
    0,    // King
};

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

int move_score(const Board& board, const Move& m, const Move* hash_move) {
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
  }

  if (m.flag == MoveFlag::Promotion || m.flag == MoveFlag::PromotionCapture) {
    score += 50000 + kPieceValues[static_cast<int>(m.promotion)];
  }

  score += square_index(m.to) - square_index(m.from);
  return score;
}

std::vector<Move> ordered_moves(const Board& board, const MoveList& moves, bool tactical_only,
                                const Move* hash_move) {
  std::vector<std::pair<int, Move>> scored;
  scored.reserve(moves.count);
  for (int i = 0; i < moves.count; ++i) {
    const Move m = moves.moves[i];
    if (tactical_only && !is_tactical_move(m)) {
      continue;
    }
    scored.push_back({move_score(board, m, hash_move), m});
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

bool quiescence_ordered(Board& board, int alpha, int beta, SearchState& st, int& out_score) {
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
  const std::vector<Move> moves = ordered_moves(board, legal, !in_check, hash_ptr);
  bool searched_any = false;
  for (const Move& move : moves) {
    searched_any = true;
    Undo undo;
    board.make_move(move, undo);
    int child_score = 0;
    if (!quiescence_ordered(board, -beta, -alpha, st, child_score)) {
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

bool negamax_v14_tt_impl(Board& board, int depth, int alpha, int beta, SearchState& st,
                         int extensions, int& out_score) {
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
    return quiescence_ordered(board, alpha, beta, st, out_score);
  }

  const int alpha_orig = alpha;
  const Move hash_move = st.tt != nullptr ? st.tt->hash_move(key) : Move{};
  const Move* hash_ptr = hash_move.from != Square::None ? &hash_move : nullptr;
  const std::vector<Move> moves = ordered_moves(board, legal, false, hash_ptr);

  int best = -kMateScore;
  Move best_move{};
  bool have_best_move = false;
  bool cutoff = false;
  Move cutoff_move{};

  for (const Move& move : moves) {
    Undo undo;
    board.make_move(move, undo);

    int child_depth = depth - 1;
    int child_extensions = extensions;
    const bool gives_check = board.in_check(board.side_to_move());
    if (gives_check && extensions < kMaxCheckExtensions) {
      child_depth = depth;
      child_extensions = extensions + 1;
    }

    int child_score = 0;
    if (!negamax_v14_tt_impl(board, child_depth, -beta, -alpha, st, child_extensions,
                             child_score)) {
      board.unmake_move(move, undo);
      return false;
    }
    board.unmake_move(move, undo);

    const int score = -child_score;
    if (score > best) {
      best = score;
      best_move = move;
      have_best_move = true;
    }
    alpha = std::max(alpha, score);
    if (alpha >= beta) {
      cutoff = true;
      cutoff_move = move;
      break;
    }
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

bool negamax_v14_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score) {
  return negamax_v14_tt_impl(board, depth, alpha, beta, st, 0, out_score);
}

}  // namespace engine::search_common
