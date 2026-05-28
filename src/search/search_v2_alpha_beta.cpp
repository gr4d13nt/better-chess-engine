#include "search_common.hpp"

#include <algorithm>

namespace engine::search_common {

bool negamax_v2_alpha_beta(Board& board, int depth, int alpha, int beta, SearchState& st,
                           int& out_score) {
  if (should_stop(st)) {
    return false;
  }
  st.nodes++;

  MoveList moves;
  generate_legal_moves(board, moves);

  if (depth == 0) {
    out_score = evaluate_for_side_to_move(board, st.eval_profile);
    return true;
  }

  if (moves.count == 0) {
    if (board.in_check(board.side_to_move())) {
      out_score = -kMateScore + (8 - depth);
      return true;
    }
    out_score = kDrawScore;
    return true;
  }

  int best = -kMateScore;
  for (int i = 0; i < moves.count; ++i) {
    const Move move = moves.moves[i];
    Undo undo;
    board.make_move(move, undo);
    int child_score = 0;
    if (!negamax_v2_alpha_beta(board, depth - 1, -beta, -alpha, st, child_score)) {
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
