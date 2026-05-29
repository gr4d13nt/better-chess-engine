#pragma once

#include <cstdint>

namespace engine {

class Board;

void init_zobrist();
std::uint64_t zobrist_hash(const Board& board);

}  // namespace engine
