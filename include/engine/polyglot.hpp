#pragma once

#include "engine/board.hpp"
#include "engine/move.hpp"

#include <cstdint>
#include <optional>

namespace engine {

// Polyglot Zobrist hash (standard opening-book format; not engine TT hash).
std::uint64_t polyglot_hash(const Board& board);

// Map a Polyglot raw move word to a legal engine move, or nullopt.
std::optional<Move> polyglot_move_to_engine(const Board& board, std::uint16_t raw_move);

}  // namespace engine
