#pragma once

#include "engine/board.hpp"
#include "engine/move.hpp"

namespace engine {

// Static exchange evaluation for a capture (or promotion capture / en passant).
// Positive = profitable for side to move; negative = losing capture.
int see_capture(const Board& board, const Move& m);

}  // namespace engine
