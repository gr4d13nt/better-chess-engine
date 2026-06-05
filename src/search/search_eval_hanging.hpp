#pragma once

#include "engine/board.hpp"

namespace engine::search_common {

// Undefended / under-defended non-pawn pieces (MG-heavy). Complements qsearch; not mobility.
void evaluate_hanging_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                            int& eg_black);

}  // namespace engine::search_common
