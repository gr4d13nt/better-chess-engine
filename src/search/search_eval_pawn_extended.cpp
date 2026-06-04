#include "search_eval_pawn_extended.hpp"

namespace engine::search_common {
namespace {

constexpr Bitboard file_bb(int file) { return bb::kFileA << file; }

constexpr Bitboard rank_bb(int rank) { return bb::kRank1 << (rank * 8); }

constexpr int kDoubledMgPerExtra = 15;
constexpr int kDoubledHomeRankMg = 8;

constexpr int kIsolatedMg = 18;
constexpr int kBackwardMg = 14;
constexpr int kBackwardEg = 6;

constexpr int kIslandPenaltyMg = 12;

constexpr int kCandidateMgByAdvance[8] = {0, 3, 5, 8, 12, 18, 26, 0};
constexpr int kCandidateEgByAdvance[8] = {0, 4, 8, 14, 22, 34, 50, 0};

Bitboard ranks_from(int rank, Color c) {
  if (rank < 0 || rank > 7) {
    return 0;
  }
  Bitboard mask = 0;
  if (c == Color::White) {
    for (int r = rank; r < 8; ++r) {
      mask |= rank_bb(r);
    }
  } else {
    for (int r = rank; r >= 0; --r) {
      mask |= rank_bb(r);
    }
  }
  return mask;
}

bool friendly_pawn_on_adjacent_file(const Board& board, Color c, int file) {
  const Bitboard pawns = board.pieces(c, PieceType::Pawn);
  if (file > 0 && (pawns & file_bb(file - 1))) {
    return true;
  }
  if (file < 7 && (pawns & file_bb(file + 1))) {
    return true;
  }
  return false;
}

bool is_passed_pawn(const Board& board, Color c, Square sq) {
  const int file = bb::file_of(sq);
  const int rank = bb::rank_of(sq);
  const Color them = !c;

  Bitboard zone = file_bb(file);
  if (file > 0) {
    zone |= file_bb(file - 1);
  }
  if (file < 7) {
    zone |= file_bb(file + 1);
  }

  const Bitboard enemy_pawns = board.pieces(them, PieceType::Pawn) & zone;
  if (c == Color::White) {
    return (enemy_pawns & ranks_from(rank + 1, Color::White)) == 0;
  }
  return (enemy_pawns & ranks_from(rank - 1, Color::Black)) == 0;
}

bool has_friendly_pawn_behind_on_adjacent_file(const Board& board, Color c, Square sq) {
  const int file = bb::file_of(sq);
  const int rank = bb::rank_of(sq);
  const Bitboard pawns = board.pieces(c, PieceType::Pawn);
  for (int df = -1; df <= 1; df += 2) {
    const int f = file + df;
    if (f < 0 || f > 7) {
      continue;
    }
    Bitboard scan = pawns & file_bb(f);
    while (scan) {
      const Square psq = bb::pop_lsb(scan);
      const int pr = bb::rank_of(psq);
      if (c == Color::White) {
        if (pr < rank) {
          return true;
        }
      } else if (pr > rank) {
        return true;
      }
    }
  }
  return false;
}

bool has_friendly_pawn_ahead_on_adjacent_file(const Board& board, Color c, Square sq) {
  const int file = bb::file_of(sq);
  const int rank = bb::rank_of(sq);
  const Bitboard pawns = board.pieces(c, PieceType::Pawn);
  for (int df = -1; df <= 1; df += 2) {
    const int f = file + df;
    if (f < 0 || f > 7) {
      continue;
    }
    Bitboard scan = pawns & file_bb(f);
    while (scan) {
      const Square psq = bb::pop_lsb(scan);
      const int pr = bb::rank_of(psq);
      if (c == Color::White) {
        if (pr > rank) {
          return true;
        }
      } else if (pr < rank) {
        return true;
      }
    }
  }
  return false;
}

bool is_backward_pawn(const Board& board, Color c, Square sq) {
  if (is_passed_pawn(board, c, sq)) {
    return false;
  }
  if (!has_friendly_pawn_ahead_on_adjacent_file(board, c, sq)) {
    return false;
  }
  return !has_friendly_pawn_behind_on_adjacent_file(board, c, sq);
}

bool is_candidate_passed_pawn(const Board& board, Color c, Square sq) {
  if (is_passed_pawn(board, c, sq)) {
    return false;
  }
  const int rank = bb::rank_of(sq);
  const int ahead_rank = c == Color::White ? rank + 1 : rank - 1;
  if (ahead_rank < 0 || ahead_rank > 7) {
    return false;
  }
  const Square ahead = static_cast<Square>(ahead_rank * 8 + bb::file_of(sq));
  return is_passed_pawn(board, c, ahead);
}

int count_pawn_islands(const Board& board, Color c) {
  const Bitboard pawns = board.pieces(c, PieceType::Pawn);
  bool has_file[8] = {};
  Bitboard scan = pawns;
  while (scan) {
    has_file[bb::file_of(bb::pop_lsb(scan))] = true;
  }

  int islands = 0;
  bool visited[8] = {};
  for (int f = 0; f < 8; ++f) {
    if (!has_file[f] || visited[f]) {
      continue;
    }
    ++islands;
    int stack[8];
    int top = 0;
    stack[top++] = f;
    visited[f] = true;
    while (top > 0) {
      const int cur = stack[--top];
      if (cur > 0 && has_file[cur - 1] && !visited[cur - 1]) {
        visited[cur - 1] = true;
        stack[top++] = cur - 1;
      }
      if (cur < 7 && has_file[cur + 1] && !visited[cur + 1]) {
        visited[cur + 1] = true;
        stack[top++] = cur + 1;
      }
    }
  }
  return islands;
}

void pawn_doubled_side(const Board& board, Color c, int& mg, int& eg) {
  mg = 0;
  eg = 0;
  (void)eg;

  const Bitboard pawns = board.pieces(c, PieceType::Pawn);
  int files_count[8] = {};
  Bitboard scan = pawns;
  while (scan) {
    ++files_count[bb::file_of(bb::pop_lsb(scan))];
  }

  for (int file = 0; file < 8; ++file) {
    if (files_count[file] > 1) {
      mg -= kDoubledMgPerExtra * (files_count[file] - 1);
    }
  }

  scan = pawns;
  while (scan) {
    const Square sq = bb::pop_lsb(scan);
    const int file = bb::file_of(sq);
    const int rank = bb::rank_of(sq);
    const int home_rank = c == Color::White ? 1 : 6;
    if (rank == home_rank && files_count[file] > 1) {
      mg -= kDoubledHomeRankMg;
    }
  }
}

void pawn_extended_side(const Board& board, Color c, int& mg, int& eg) {
  mg = 0;
  eg = 0;

  const Bitboard pawns = board.pieces(c, PieceType::Pawn);
  Bitboard scan = pawns;
  while (scan) {
    const Square sq = bb::pop_lsb(scan);
    const int file = bb::file_of(sq);
    const int rank = bb::rank_of(sq);

    if (!friendly_pawn_on_adjacent_file(board, c, file)) {
      mg -= kIsolatedMg;
    }

    if (is_backward_pawn(board, c, sq)) {
      mg -= kBackwardMg;
      eg -= kBackwardEg;
    }

    if (is_candidate_passed_pawn(board, c, sq)) {
      const int advance = c == Color::White ? rank : (7 - rank);
      mg += kCandidateMgByAdvance[advance];
      eg += kCandidateEgByAdvance[advance];
    }
  }

  const int islands = count_pawn_islands(board, c);
  if (islands > 1) {
    mg -= kIslandPenaltyMg * (islands - 1);
  }
}

}  // namespace

void evaluate_pawn_doubled_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                                 int& eg_black) {
  pawn_doubled_side(board, Color::White, mg_white, eg_white);
  pawn_doubled_side(board, Color::Black, mg_black, eg_black);
}

void evaluate_pawn_extended_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                                  int& eg_black) {
  pawn_extended_side(board, Color::White, mg_white, eg_white);
  pawn_extended_side(board, Color::Black, mg_black, eg_black);
}

}  // namespace engine::search_common
