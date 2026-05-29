#include "search_eval_rook.hpp"

namespace engine::search_common {

namespace {

constexpr Bitboard file_bb(int file) { return bb::kFileA << file; }

constexpr Bitboard rank_bb(int rank) { return bb::kRank1 << (rank * 8); }

// File structure only; 7th-rank activity is already in kRookPst + rook mobility.
constexpr int kOpenFileMg = 14;
constexpr int kOpenFileEg = 5;
constexpr int kSemiOpenFileMg = 8;
constexpr int kSemiOpenFileEg = 3;

static bool rook_is_advanced(Color c, int rank) {
  return c == Color::White ? rank >= 3 : rank <= 4;
}

static bool is_open_file(const Board& board, int file) {
  const Bitboard mask = file_bb(file);
  const Bitboard pawns = board.pieces(Color::White, PieceType::Pawn) |
                         board.pieces(Color::Black, PieceType::Pawn);
  return (pawns & mask) == 0;
}

static bool is_semi_open_file(const Board& board, Color c, int file) {
  const Bitboard mask = file_bb(file);
  if (board.pieces(c, PieceType::Pawn) & mask) {
    return false;
  }
  return (board.pieces(!c, PieceType::Pawn) & mask) != 0;
}

static bool friendly_pawn_behind_rook(const Board& board, Color c, int file, int rank) {
  const Bitboard pawns = board.pieces(c, PieceType::Pawn) & file_bb(file);
  if (c == Color::White) {
    Bitboard behind = 0;
    for (int r = 0; r < rank; ++r) {
      behind |= rank_bb(r);
    }
    return (pawns & behind) != 0;
  }
  Bitboard behind = 0;
  for (int r = rank + 1; r < 8; ++r) {
    behind |= rank_bb(r);
  }
  return (pawns & behind) != 0;
}

static void rook_placement_for_color(const Board& board, Color c, int& mg, int& eg) {
  mg = 0;
  eg = 0;

  Bitboard rooks = board.pieces(c, PieceType::Rook);
  while (rooks) {
    const Square sq = bb::pop_lsb(rooks);
    const int file = bb::file_of(sq);
    const int rank = bb::rank_of(sq);

    if (!rook_is_advanced(c, rank)) {
      continue;
    }
    if (friendly_pawn_behind_rook(board, c, file, rank)) {
      continue;
    }

    if (is_open_file(board, file)) {
      mg += kOpenFileMg;
      eg += kOpenFileEg;
    } else if (is_semi_open_file(board, c, file)) {
      mg += kSemiOpenFileMg;
      eg += kSemiOpenFileEg;
    }
  }
}

}  // namespace

void evaluate_rook_placement_split(const Board& board, int& mg_white, int& eg_white,
                                   int& mg_black, int& eg_black) {
  rook_placement_for_color(board, Color::White, mg_white, eg_white);
  rook_placement_for_color(board, Color::Black, mg_black, eg_black);
}

}  // namespace engine::search_common
