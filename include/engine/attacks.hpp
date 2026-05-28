#pragma once

#include "engine/bitboard.hpp"
#include "engine/types.hpp"

namespace engine {

// Call once at startup before using attack lookups or move generation.
void init_attacks();

Bitboard knight_attacks(Square sq);
Bitboard king_attacks(Square sq);
Bitboard pawn_attacks(Color color, Square sq);
Bitboard bishop_attacks(Square sq, Bitboard occupied);
Bitboard rook_attacks(Square sq, Bitboard occupied);
Bitboard queen_attacks(Square sq, Bitboard occupied);

}  // namespace engine
