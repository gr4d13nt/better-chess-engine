#include "search_common.hpp"

#include "search_tt.hpp"

#include <algorithm>
#include <vector>

namespace engine::search_common {
namespace {

constexpr int kPieceValues[kNumPieceTypes] = {
    100,  // Pawn
    320,  // Knight
    330,  // Bishop
    500,  // Rook
    900,  // Queen
    0,    // King
};

bool is_tactical_move(const Move& m) {
  return m.flag == MoveFlag::Capture || m.flag == MoveFlag::PromotionCapture ||
         m.flag == MoveFlag::EnPassant || m.flag == MoveFlag::Promotion;
}

int move_score(const Board& board, const Move& m) {
  const Piece moving_piece = board.piece_on(m.from);
  const PieceType moving_type = piece_type(moving_piece);

  int score = 0;
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

std::vector<Move> ordered_moves(const Board& board, const MoveList& moves, bool tactical_only) {
  std::vector<std::pair<int, Move>> scored;
  scored.reserve(moves.count);
  for (int i = 0; i < moves.count; ++i) {
    const Move m = moves.moves[i];
    if (tactical_only && !is_tactical_move(m)) {
      continue;
    }
    scored.push_back({move_score(board, m), m});
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

  const std::vector<Move> moves = ordered_moves(board, legal, !in_check);
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

}  // namespace

bool negamax_v10_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score) {
  if (should_stop(st)) {
    return false;
  }
  st.nodes++;

  const std::uint64_t key = board.zobrist();
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
  const std::vector<Move> moves = ordered_moves(board, legal, false);
  int best = -kMateScore;
  for (const Move& move : moves) {
    Undo undo;
    board.make_move(move, undo);
    int child_score = 0;
    if (!negamax_v10_tt(board, depth - 1, -beta, -alpha, st, child_score)) {
      board.unmake_move(move, undo);
      return false;
    }
    board.unmake_move(move, undo);

    const int score = -child_score;
    best = std::max(best, score);
    alpha = std::max(alpha, score);
    if (alpha >= beta) {
      break;
    }
  }

  if (st.tt != nullptr) {
    TTBound bound = TTBound::Exact;
    if (best <= alpha_orig) {
      bound = TTBound::Upper;
    } else if (best >= beta) {
      bound = TTBound::Lower;
    }
    st.tt->store(key, depth, best, bound, st.tt_generation);
  }

  out_score = best;
  return true;
}

}  // namespace engine::search_common
