#pragma once

#include "engine/board.hpp"

namespace engine::search_common {

// Endgame mop-up (White - Black): king approach + drive enemy king to the edge.
int evaluate_mop_up_net(const Board& board);

}  // namespace engine::search_common
