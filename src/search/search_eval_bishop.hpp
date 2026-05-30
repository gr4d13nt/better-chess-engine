#pragma once

#include "engine/board.hpp"

namespace engine::search_common {

// Penalize bishops blocked by own pawns on the same color complex (White, then Black).
void evaluate_bishop_freedom_split(const Board& board, int& mg_white, int& eg_white,
                                   int& mg_black, int& eg_black);

}  // namespace engine::search_common
