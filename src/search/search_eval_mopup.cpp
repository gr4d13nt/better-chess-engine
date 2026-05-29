#include "search_eval_mopup.hpp"

#include "engine/bitboard.hpp"
#include "search_eval_phase.hpp"

#include <algorithm>
#include <cstdlib>

namespace engine::search_common {
namespace {

constexpr int kPawnValue = 100;
constexpr int kKnightValue = 320;
constexpr int kBishopValue = 330;
constexpr int kRookValue = 500;
constexpr int kQueenValue = 900;

int piece_material_value(PieceType pt) {
  switch (pt) {
    case PieceType::Pawn:
      return kPawnValue;
    case PieceType::Knight:
      return kKnightValue;
    case PieceType::Bishop:
      return kBishopValue;
    case PieceType::Rook:
      return kRookValue;
    case PieceType::Queen:
      return kQueenValue;
    default:
      return 0;
  }
}

int material_score(const Board& board, Color c) {
  int score = 0;
  for (int pt = static_cast<int>(PieceType::Pawn); pt <= static_cast<int>(PieceType::Queen); ++pt) {
    const int count = bb::popcount(board.pieces(c, static_cast<PieceType>(pt)));
    score += count * piece_material_value(static_cast<PieceType>(pt));
  }
  return score;
}

Square king_square(const Board& board, Color c) {
  const Bitboard kings = board.pieces(c, PieceType::King);
  return static_cast<Square>(bb::lsb_index(kings));
}

int manhattan_between(Square a, Square b) {
  return std::abs(bb::file_of(a) - bb::file_of(b)) + std::abs(bb::rank_of(a) - bb::rank_of(b));
}

int manhattan_from_centre(Square sq) {
  const int f = bb::file_of(sq);
  const int r = bb::rank_of(sq);
  const int d4 = std::abs(f - 3) + std::abs(r - 3);
  const int e4 = std::abs(f - 4) + std::abs(r - 3);
  const int d5 = std::abs(f - 3) + std::abs(r - 4);
  const int e5 = std::abs(f - 4) + std::abs(r - 4);
  return std::min({d4, e4, d5, e5});
}

int endgame_weight(const Board& board) {
  const int phase = game_phase(board);
  return kPhaseMax - phase;
}

bool mop_up_active_for_color(const Board& board, Color us) {
  const Color them = !us;
  const int my_material = material_score(board, us);
  const int enemy_material = material_score(board, them);
  const int superior_threshold = enemy_material + kPawnValue * 2;
  return endgame_weight(board) > 0 && my_material > superior_threshold;
}

int mop_up_for_color(const Board& board, Color us) {
  if (!mop_up_active_for_color(board, us)) {
    return 0;
  }

  const Color them = !us;
  const Square my_king = king_square(board, us);
  const Square enemy_king = king_square(board, them);
  const int endgame_t = endgame_weight(board);

  int mop_up = 0;
  mop_up += (14 - manhattan_between(my_king, enemy_king)) * 4;
  mop_up += manhattan_from_centre(enemy_king) * 10;
  return (mop_up * endgame_t) / kPhaseMax;
}

}  // namespace

int evaluate_mop_up_net(const Board& board) {
  return mop_up_for_color(board, Color::White) - mop_up_for_color(board, Color::Black);
}

}  // namespace engine::search_common
