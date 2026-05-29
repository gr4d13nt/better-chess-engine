#include "search_common.hpp"

namespace engine::search_common {

void prioritize_move(MoveList& moves, const Move& prefer) {
  if (prefer.from == Square::None) {
    return;
  }
  for (int i = 0; i < moves.count; ++i) {
    if (moves_equal(moves.moves[i], prefer)) {
      if (i > 0) {
        const Move tmp = moves.moves[0];
        moves.moves[0] = moves.moves[i];
        moves.moves[i] = tmp;
      }
      return;
    }
  }
}

}  // namespace engine::search_common
