#pragma once

#include "engine/search.hpp"

namespace engine {

// Single-threaded search (all versions). Used by lazy SMP workers.
SearchResult search_best_move_single(Board& board, const SearchConfig& cfg);

}  // namespace engine
