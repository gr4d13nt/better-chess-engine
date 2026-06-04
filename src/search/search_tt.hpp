#pragma once

#include "engine/move.hpp"

#include <cstdint>
#include <vector>

namespace engine::search_common {

enum class TTBound : std::uint8_t { None, Exact, Lower, Upper };

struct TTEntry {
  std::uint64_t key = 0;
  std::int16_t score = 0;
  std::int8_t depth = -1;
  TTBound bound = TTBound::None;
  std::uint8_t generation = 0;
  Move best_move{};
};

class TranspositionTable {
 public:
  explicit TranspositionTable(std::size_t size_power_of_two = 20);

  void clear();
  // Marks a new root search; returns the generation to store/probe with.
  std::uint8_t new_search();
  bool probe(std::uint64_t key, int depth, int alpha, int beta, std::uint8_t generation,
             int& out_score) const;
  Move hash_move(std::uint64_t key) const;
  void store(std::uint64_t key, int depth, int score, TTBound bound, std::uint8_t generation,
             const Move& best_move = Move{}, bool store_move = false);

 private:
  std::vector<TTEntry> table_;
  std::size_t mask_ = 0;
  std::uint8_t generation_ = 0;
};

TranspositionTable& global_tt();
void tt_clear();

}  // namespace engine::search_common
