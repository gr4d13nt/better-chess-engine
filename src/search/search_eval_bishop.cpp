#include "search_eval_bishop.hpp"

#include "engine/bitboard.hpp"

namespace engine::search_common {

namespace {

constexpr int kSameColorPawnMg = 10;
constexpr int kSameColorPawnEg = 4;

static bool is_dark_square(Square sq) {
  return ((bb::file_of(sq) + bb::rank_of(sq)) & 1) == 0;
}

static Bitboard color_complex(Square sq) {
  const bool dark = is_dark_square(sq);
  Bitboard mask = 0;
  for (int s = 0; s < 64; ++s) {
    const Square sq2 = static_cast<Square>(s);
    if (is_dark_square(sq2) == dark) {
      mask |= bb::square_bb(sq2);
    }
  }
  return mask;
}

static void bishop_freedom_for_color(const Board& board, Color c, int& mg, int& eg) {
  mg = 0;
  eg = 0;

  const Bitboard own_pawns = board.pieces(c, PieceType::Pawn);
  Bitboard bishops = board.pieces(c, PieceType::Bishop);
  while (bishops) {
    const Square sq = bb::pop_lsb(bishops);
    const int blocked = bb::popcount(own_pawns & color_complex(sq));
    mg -= blocked * kSameColorPawnMg;
    eg -= blocked * kSameColorPawnEg;
  }
}

}  // namespace

void evaluate_bishop_freedom_split(const Board& board, int& mg_white, int& eg_white,
                                   int& mg_black, int& eg_black) {
  bishop_freedom_for_color(board, Color::White, mg_white, eg_white);
  bishop_freedom_for_color(board, Color::Black, mg_black, eg_black);
}

}  // namespace engine::search_common
