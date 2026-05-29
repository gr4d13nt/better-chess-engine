#pragma once

#include "engine/board.hpp"

namespace engine::search_common {

// 0 = endgame, kPhaseMax = middlegame (used by v6 layered terms and future v7 eval).
constexpr int kPhaseMax = 256;

int game_phase(const Board& board);
int taper_eval(int mg, int eg, int phase);

}  // namespace engine::search_common
