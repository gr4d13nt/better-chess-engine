#pragma once

#include "engine/board.hpp"

namespace engine::search_common {

// Doubled-pawn penalties only (from v6 layered; no isolated/passed).
void evaluate_pawn_doubled_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                                 int& eg_black);

// Isolated, backward, pawn islands, candidate passed (not true passed — see v21 passed pawns).
void evaluate_pawn_extended_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                                  int& eg_black);

}  // namespace engine::search_common
