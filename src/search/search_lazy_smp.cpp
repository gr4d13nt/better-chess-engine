#include "engine/search.hpp"

#include "engine/book.hpp"

#include "search_internal.hpp"

#include "search_common.hpp"
#include "search_tt.hpp"

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace engine {

namespace {

int resolve_thread_count(int requested) {
  if (requested > 0) {
    return requested;
  }
  const unsigned hw = std::thread::hardware_concurrency();
  return hw > 1 ? static_cast<int>(hw) : 1;
}

bool is_better_smp_result(const SearchResult& candidate, const SearchResult& current) {
  if (!candidate.has_move) {
    return false;
  }
  if (!current.has_move) {
    return true;
  }
  if (candidate.depth_reached != current.depth_reached) {
    return candidate.depth_reached > current.depth_reached;
  }
  return candidate.score > current.score;
}

SearchResult lazy_smp_search(Board& board, const SearchConfig& cfg, int thread_count) {
  const bool use_book = cfg.version == EngineVersion::V25_OpeningBook ||
                        cfg.version == EngineVersion::V26_LMR ||
                        cfg.version == EngineVersion::V27_PVS ||
                        cfg.version == EngineVersion::V28_FutilityLmp ||
                        cfg.version == EngineVersion::V29_LazySMP ||
                        cfg.version == EngineVersion::V30_TimeManagement ||
                        cfg.use_opening_book;
  if (use_book) {
    if (const auto book_move = probe_opening_book(board)) {
      SearchResult result{};
      result.best_move = *book_move;
      result.has_move = true;
      result.nodes = 0;
      return result;
    }
  }

  std::atomic<bool> shared_stop{false};
  std::vector<SearchResult> results(static_cast<std::size_t>(thread_count));
  std::vector<std::unique_ptr<search_common::TranspositionTable>> tables;
  tables.reserve(static_cast<std::size_t>(thread_count));

  std::vector<std::thread> workers;
  workers.reserve(static_cast<std::size_t>(thread_count));

  for (int i = 0; i < thread_count; ++i) {
    tables.push_back(std::make_unique<search_common::TranspositionTable>(20));
  }

  for (int i = 0; i < thread_count; ++i) {
    workers.emplace_back([&, i]() {
      Board worker_board = board;
      SearchConfig worker_cfg = cfg;
      worker_cfg.local_tt = tables[static_cast<std::size_t>(i)].get();
      worker_cfg.shared_stop = &shared_stop;
      worker_cfg.clear_transposition_table = false;
      worker_cfg.smp_depth_offset = (i & 1);
      results[static_cast<std::size_t>(i)] = search_best_move_single(worker_board, worker_cfg);
    });
  }

  for (std::thread& worker : workers) {
    worker.join();
  }

  SearchResult best{};
  int total_nodes = 0;
  for (const SearchResult& result : results) {
    if (is_better_smp_result(result, best)) {
      best = result;
    }
    total_nodes += result.nodes;
  }
  best.nodes = total_nodes;

  if (!best.has_move) {
    best = search_best_move_single(board, cfg);
  }
  return best;
}

}  // namespace

SearchResult search_best_move(Board& board, const SearchConfig& cfg) {
  if ((cfg.version == EngineVersion::V29_LazySMP ||
       cfg.version == EngineVersion::V30_TimeManagement) &&
      cfg.movetime_ms > 0) {
    const int thread_count = resolve_thread_count(cfg.num_threads);
    if (thread_count > 1) {
      return lazy_smp_search(board, cfg, thread_count);
    }
  }
  return search_best_move_single(board, cfg);
}

}  // namespace engine
