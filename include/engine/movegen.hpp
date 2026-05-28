#pragma once

#include "engine/board.hpp"
#include "engine/move.hpp"

namespace engine {

// Legal move generator for the current side.
void generate_legal_moves(const Board& board, MoveList& out);

// Attackers of a square for a given color (includes blockers on ray).
Bitboard attackers_to(const Board& board, Square sq, Color by_color);

}  // namespace engine
