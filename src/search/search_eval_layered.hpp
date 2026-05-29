#pragma once

#include "engine/board.hpp"

namespace engine::search_common {

// Pawn structure + king safety, split into middlegame / endgame buckets (White, then Black).
void evaluate_layered_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                            int& eg_black);

void evaluate_pawn_structure_split(const Board& board, int& mg_white, int& eg_white,
                                   int& mg_black, int& eg_black);

void evaluate_king_safety_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                                int& eg_black);

}  // namespace engine::search_common
