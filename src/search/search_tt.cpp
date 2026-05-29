#include "search_tt.hpp"

#include "search_common.hpp"

namespace engine::search_common {
namespace {

constexpr int kMateThreshold = kMateScore - kMaxSearchDepth - 1;

bool is_mate_score(int score) {
  return score >= kMateThreshold || score <= -kMateThreshold;
}

bool has_hash_move(const Move& move) { return move.from != Square::None; }

}  // namespace

TranspositionTable::TranspositionTable(std::size_t size_power_of_two) {
  const std::size_t size = std::size_t{1} << size_power_of_two;
  table_.assign(size, TTEntry{});
  mask_ = size - 1;
}

void TranspositionTable::clear() {
  for (TTEntry& entry : table_) {
    entry = TTEntry{};
  }
}

bool TranspositionTable::probe(std::uint64_t key, int depth, int alpha, int beta,
                               int& out_score) const {
  const TTEntry& entry = table_[key & mask_];
  if (entry.key != key || entry.depth < depth) {
    return false;
  }

  out_score = entry.score;
  if (is_mate_score(out_score) && entry.bound != TTBound::Exact) {
    return false;
  }

  switch (entry.bound) {
    case TTBound::Exact:
      return true;
    case TTBound::Lower:
      return out_score >= beta;
    case TTBound::Upper:
      return out_score <= alpha;
    default:
      return false;
  }
}

Move TranspositionTable::hash_move(std::uint64_t key) const {
  const TTEntry& entry = table_[key & mask_];
  if (entry.key != key || !has_hash_move(entry.best_move)) {
    return Move{};
  }
  return entry.best_move;
}

void TranspositionTable::store(std::uint64_t key, int depth, int score, TTBound bound,
                               const Move& best_move, bool store_move) {
  TTEntry& entry = table_[key & mask_];
  if (entry.key == key && entry.depth > depth) {
    return;
  }

  entry.key = key;
  entry.depth = static_cast<std::int8_t>(depth);
  entry.score = static_cast<std::int16_t>(score);
  entry.bound = is_mate_score(score) ? TTBound::Exact : bound;
  if (store_move && has_hash_move(best_move)) {
    entry.best_move = best_move;
  }
}

TranspositionTable& global_tt() {
  static TranspositionTable table(20);
  return table;
}

void tt_clear() { global_tt().clear(); }

}  // namespace engine::search_common
