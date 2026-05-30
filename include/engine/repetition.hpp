#pragma once

#include "engine/board.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace engine {

// Zobrist hash for each position in the game (including the current root).
std::vector<std::uint64_t> repetition_hashes_from_fens(const std::vector<std::string>& fens);

}  // namespace engine
