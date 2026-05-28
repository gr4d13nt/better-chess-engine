#include "engine/search.hpp"

#include "engine/movegen.hpp"

#include <algorithm>
#include <chrono>

namespace engine {
namespace {

constexpr int kPieceValues[kNumPieceTypes] = {
    100,   // Pawn
    320,   // Knight
    330,   // Bishop
    500,   // Rook
    900,   // Queen
    0,     // King
};

constexpr int kMateScore = 100000;
constexpr int kDrawScore = 0;
constexpr int kMaxSearchDepth = 64;

struct SearchState {
  int nodes = 0;
  bool use_time = false;
  std::chrono::steady_clock::time_point deadline{};
  bool stopped = false;
};

bool should_stop(SearchState& st) {
  if (!st.use_time) {
    return false;
  }
  // Poll clock periodically to reduce overhead.
  if ((st.nodes & 2047) != 0) {
    return false;
  }
  if (std::chrono::steady_clock::now() >= st.deadline) {
    st.stopped = true;
    return true;
  }
  return false;
}

int evaluate_for_side_to_move(const Board& board) {
  const int white_eval = evaluate(board);
  return board.side_to_move() == Color::White ? white_eval : -white_eval;
}

bool negamax_no_pruning(Board& board, int depth, SearchState& st, int& out_score) {
  if (should_stop(st)) {
    return false;
  }
  st.nodes++;

  MoveList moves;
  generate_legal_moves(board, moves);

  if (depth == 0) {
    out_score = evaluate_for_side_to_move(board);
    return true;
  }

  if (moves.count == 0) {
    if (board.in_check(board.side_to_move())) {
      out_score = -kMateScore + (8 - depth);
      return true;
    }
    out_score = kDrawScore;
    return true;
  }

  int best = -kMateScore;
  for (int i = 0; i < moves.count; ++i) {
    const Move move = moves.moves[i];
    Undo undo;
    board.make_move(move, undo);
    int child_score = 0;
    if (!negamax_no_pruning(board, depth - 1, st, child_score)) {
      board.unmake_move(move, undo);
      return false;
    }
    board.unmake_move(move, undo);
    const int score = -child_score;
    best = std::max(best, score);
  }
  out_score = best;
  return true;
}

bool negamax_alpha_beta(Board& board, int depth, int alpha, int beta, SearchState& st, int& out_score) {
  if (should_stop(st)) {
    return false;
  }
  st.nodes++;

  MoveList moves;
  generate_legal_moves(board, moves);

  if (depth == 0) {
    out_score = evaluate_for_side_to_move(board);
    return true;
  }

  if (moves.count == 0) {
    if (board.in_check(board.side_to_move())) {
      out_score = -kMateScore + (8 - depth);
      return true;
    }
    out_score = kDrawScore;
    return true;
  }

  int best = -kMateScore;
  for (int i = 0; i < moves.count; ++i) {
    const Move move = moves.moves[i];
    Undo undo;
    board.make_move(move, undo);
    int child_score = 0;
    if (!negamax_alpha_beta(board, depth - 1, -beta, -alpha, st, child_score)) {
      board.unmake_move(move, undo);
      return false;
    }
    board.unmake_move(move, undo);
    const int score = -child_score;

    best = std::max(best, score);
    alpha = std::max(alpha, score);
    if (alpha >= beta) {
      break;
    }
  }
  out_score = best;
  return true;
}

}  // namespace

int evaluate(const Board& board) {
  int white = 0;
  int black = 0;

  for (int pt = 0; pt < kNumPieceTypes; ++pt) {
    const int value = kPieceValues[pt];
    white += bb::popcount(board.pieces(Color::White, static_cast<PieceType>(pt))) * value;
    black += bb::popcount(board.pieces(Color::Black, static_cast<PieceType>(pt))) * value;
  }

  return white - black;
}

SearchResult search_best_move(Board& board, const SearchConfig& cfg) {
  MoveList moves;
  generate_legal_moves(board, moves);
  if (moves.count == 0) {
    SearchResult result{};
    result.score = board.in_check(board.side_to_move()) ? -kMateScore : kDrawScore;
    result.nodes = 1;
    return result;
  }

  auto run_depth = [&](int depth, SearchState& st, SearchResult& out) -> bool {
    out = SearchResult{};
    out.score = -kMateScore;
    int root_alpha = -kMateScore;
    const int root_beta = kMateScore;

    for (int i = 0; i < moves.count; ++i) {
      const Move move = moves.moves[i];
      Undo undo;
      board.make_move(move, undo);
      int child_score = 0;
      bool ok = false;
      if (cfg.version == EngineVersion::V2_AlphaBeta) {
        ok = negamax_alpha_beta(board, depth - 1, -root_beta, -root_alpha, st, child_score);
      } else {
        ok = negamax_no_pruning(board, depth - 1, st, child_score);
      }
      board.unmake_move(move, undo);
      if (!ok) {
        return false;
      }

      const int score = -child_score;
      if (!out.has_move || score > out.score) {
        out.has_move = true;
        out.best_move = move;
        out.score = score;
      }
      root_alpha = std::max(root_alpha, score);
    }
    out.nodes = st.nodes;
    return true;
  };

  // Depth-only mode (existing behavior).
  if (cfg.movetime_ms <= 0) {
    SearchState st{};
    SearchResult result{};
    const int depth = std::max(1, cfg.depth);
    const bool ok = run_depth(depth, st, result);
    result.nodes = st.nodes;
    if (!ok || !result.has_move) {
      result.has_move = true;
      result.best_move = moves.moves[0];
      result.score = evaluate_for_side_to_move(board);
    }
    return result;
  }

  // Timed mode: iterative deepening with deadline.
  SearchState st{};
  st.use_time = true;
  st.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(cfg.movetime_ms);

  SearchResult best_complete{};
  const int max_depth = cfg.depth > 0 ? cfg.depth : kMaxSearchDepth;
  for (int d = 1; d <= max_depth; ++d) {
    SearchResult current{};
    if (!run_depth(d, st, current)) {
      break;
    }
    best_complete = current;
  }

  best_complete.nodes = st.nodes;
  if (!best_complete.has_move) {
    best_complete.has_move = true;
    best_complete.best_move = moves.moves[0];
    best_complete.score = evaluate_for_side_to_move(board);
  }
  return best_complete;
}

}  // namespace engine
