#pragma once

#include "engine/board.hpp"

namespace engine::search_common {

// Pawn-dominated central territory (MG-heavy, tapered by phase). No overlap with piece mobility.
void evaluate_space_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                          int& eg_black);

}  // namespace engine::search_common
