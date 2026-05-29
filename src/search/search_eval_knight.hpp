#pragma once

#include "engine/board.hpp"

namespace engine::search_common {

// Knight outpost bonuses (White, then Black), split into MG / EG buckets.
void evaluate_knight_outposts_split(const Board& board, int& mg_white, int& eg_white,
                                    int& mg_black, int& eg_black);

}  // namespace engine::search_common
