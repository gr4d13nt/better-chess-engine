#include "search_common.hpp"

#include <algorithm>

namespace engine::search_common {
namespace {

bool is_tactical_move(const Move& m) {
  return m.flag == MoveFlag::Capture || m.flag == MoveFlag::PromotionCapture ||
         m.flag == MoveFlag::EnPassant || m.flag == MoveFlag::Promotion;
}

bool quiescence(Board& board, int alpha, int beta, SearchState& st, int& out_score) {
  if (should_stop(st)) {
    return false;
  }
  st.nodes++;

  MoveList moves;
  generate_legal_moves(board, moves);

  // Terminal detection still applies in qsearch.
  if (moves.count == 0) {
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

  bool searched_any = false;
  for (int i = 0; i < moves.count; ++i) {
    const Move move = moves.moves[i];
    if (!in_check && !is_tactical_move(move)) {
      continue;
    }
    searched_any = true;

    Undo undo;
    board.make_move(move, undo);
    int child_score = 0;
    if (!quiescence(board, -beta, -alpha, st, child_score)) {
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

bool negamax_v4_quiescence(Board& board, int depth, int alpha, int beta, SearchState& st,
                           int& out_score) {
  if (should_stop(st)) {
    return false;
  }
  st.nodes++;

  MoveList moves;
  generate_legal_moves(board, moves);

  if (moves.count == 0) {
    if (board.in_check(board.side_to_move())) {
      out_score = -kMateScore + (8 - depth);
      return true;
    }
    out_score = kDrawScore;
    return true;
  }

  if (depth == 0) {
    return quiescence(board, alpha, beta, st, out_score);
  }

  int best = -kMateScore;
  for (int i = 0; i < moves.count; ++i) {
    const Move move = moves.moves[i];
    Undo undo;
    board.make_move(move, undo);
    int child_score = 0;
    if (!negamax_v4_quiescence(board, depth - 1, -beta, -alpha, st, child_score)) {
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
  out_score = best;
  return true;
}

}  // namespace engine::search_common
