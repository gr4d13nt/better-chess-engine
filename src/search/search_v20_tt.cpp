#include "search_common.hpp"

namespace engine::search_common {

// v20 uses the v19 search kernel; only TT persistence differs (see search_dispatch.cpp).
bool negamax_v20_tt(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score) {
  return negamax_v19_tt(board, depth, alpha, beta, st, out_score);
}

}  // namespace engine::search_common
