#pragma once

#include "engine/board.hpp"
#include "engine/move.hpp"

#include <cstddef>
#include <optional>

namespace engine {

// Loads Polyglot .bin (default) or legacy UCI-line .txt from path. Idempotent.
void init_opening_book(const char* path = nullptr);

bool opening_book_loaded();

// Number of raw Polyglot entries (0 when using text book).
std::size_t opening_book_entry_count();

// Weighted-random book move for the current position, or nullopt if out of book.
std::optional<Move> probe_opening_book(const Board& board);

}  // namespace engine
