#include "search_common.hpp"

namespace engine::search_common {

// v21 uses the v20 search kernel; eval gains come from passed-pawn terms.
bool negamax_v21_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score) {
  return negamax_v20_tt(board, depth, alpha, beta, st, out_score);
}

}  // namespace engine::search_common
