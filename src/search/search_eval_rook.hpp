#pragma once

#include "engine/board.hpp"

namespace engine::search_common {

// Rook on open/semi-open files and 7th/2nd-rank placement (White, then Black).
void evaluate_rook_placement_split(const Board& board, int& mg_white, int& eg_white,
                                   int& mg_black, int& eg_black);

}  // namespace engine::search_common
