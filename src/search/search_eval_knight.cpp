#include "search_eval_knight.hpp"

#include "engine/attacks.hpp"
#include "engine/movegen.hpp"

namespace engine::search_common {

namespace {

constexpr Bitboard file_bb(int file) { return bb::kFileA << file; }

constexpr Bitboard rank_bb(int rank) { return bb::kRank1 << (rank * 8); }

constexpr int kOutpostMg = 20;
constexpr int kOutpostEg = 8;
constexpr int kSupportedMg = 10;
constexpr int kSupportedEg = 4;
constexpr int kCentralFileMg = 6;

static bool square_attacked_by_enemy_pawn(const Board& board, Color c, Square sq) {
  const Color them = !c;
  return (attackers_to(board, sq, them) & board.pieces(them, PieceType::Pawn)) != 0;
}

// Enemy pawn on the same file, on the far side of the knight, can still kick it forward.
static bool enemy_pawn_blocks_file(const Board& board, Color c, Square sq) {
  const int file = bb::file_of(sq);
  const int rank = bb::rank_of(sq);
  const Color them = !c;
  const Bitboard enemy_pawns = board.pieces(them, PieceType::Pawn) & file_bb(file);

  if (c == Color::White) {
    Bitboard ahead = 0;
    for (int r = rank + 1; r < 8; ++r) {
      ahead |= rank_bb(r);
    }
    return (enemy_pawns & ahead) != 0;
  }

  Bitboard ahead = 0;
  for (int r = 0; r < rank; ++r) {
    ahead |= rank_bb(r);
  }
  return (enemy_pawns & ahead) != 0;
}

static bool knight_on_outpost(const Board& board, Color c, Square sq) {
  const int rank = bb::rank_of(sq);

  if (c == Color::White) {
    if (rank < 3) {
      return false;
    }
  } else if (rank > 4) {
    return false;
  }

  if (square_attacked_by_enemy_pawn(board, c, sq)) {
    return false;
  }
  if (enemy_pawn_blocks_file(board, c, sq)) {
    return false;
  }
  return true;
}

static bool pawn_supports_knight(const Board& board, Color c, Square sq) {
  const int file = bb::file_of(sq);
  const int rank = bb::rank_of(sq);
  const Bitboard pawns = board.pieces(c, PieceType::Pawn);

  for (int df = -1; df <= 1; ++df) {
    const int f = file + df;
    if (f < 0 || f > 7) {
      continue;
    }
    if (c == Color::White) {
      if (rank > 0 && (pawns & bb::square_bb(bb::make_square(f, rank - 1)))) {
        return true;
      }
    } else if (rank < 7 && (pawns & bb::square_bb(bb::make_square(f, rank + 1)))) {
      return true;
    }
  }
  return false;
}

static void knight_outposts_for_color(const Board& board, Color c, int& mg, int& eg) {
  mg = 0;
  eg = 0;

  Bitboard knights = board.pieces(c, PieceType::Knight);
  while (knights) {
    const Square sq = bb::pop_lsb(knights);
    if (!knight_on_outpost(board, c, sq)) {
      continue;
    }

    mg += kOutpostMg;
    eg += kOutpostEg;

    const int file = bb::file_of(sq);
    if (file >= 2 && file <= 5) {
      mg += kCentralFileMg;
    }
    if (pawn_supports_knight(board, c, sq)) {
      mg += kSupportedMg;
      eg += kSupportedEg;
    }
  }
}

}  // namespace

void evaluate_knight_outposts_split(const Board& board, int& mg_white, int& eg_white,
                                    int& mg_black, int& eg_black) {
  knight_outposts_for_color(board, Color::White, mg_white, eg_white);
  knight_outposts_for_color(board, Color::Black, mg_black, eg_black);
}

}  // namespace engine::search_common
