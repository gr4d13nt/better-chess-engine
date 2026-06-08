#pragma once

#include "engine/types.hpp"

#include <cstdint>

namespace engine {

class Board;

void init_zobrist();
std::uint64_t zobrist_hash(const Board& board);

std::uint64_t zobrist_piece_key(Color color, PieceType piece_type, Square square);
std::uint64_t zobrist_side_key();
std::uint64_t zobrist_castling_key(std::uint8_t castling_rights);
std::uint64_t zobrist_ep_key(Square ep_square);

}  // namespace engine
