#include "search_eval_space.hpp"

#include "engine/attacks.hpp"
#include "search_eval_phase.hpp"

namespace engine::search_common {
namespace {

constexpr Bitboard file_bb(int file) { return bb::kFileA << file; }

constexpr Bitboard rank_bb(int rank) { return bb::kRank1 << (rank * 8); }

constexpr int kSafeSquareMg = 4;
constexpr int kCentralPawnMg = 8;
constexpr int kCentralPawnEg = 2;

Bitboard pawn_attack_mask(const Board& board, Color c) {
  Bitboard mask = 0;
  Bitboard pawns = board.pieces(c, PieceType::Pawn);
  while (pawns) {
    mask |= pawn_attacks(c, bb::pop_lsb(pawns));
  }
  return mask;
}

Bitboard space_zone_mask(Color c) {
  Bitboard files = file_bb(2) | file_bb(3) | file_bb(4) | file_bb(5);
  if (c == Color::White) {
    return files & (rank_bb(2) | rank_bb(3) | rank_bb(4) | rank_bb(5));
  }
  return files & (rank_bb(2) | rank_bb(3) | rank_bb(4) | rank_bb(5));
}

void space_side(const Board& board, Color c, int& mg, int& eg) {
  mg = 0;
  eg = 0;

  const int phase = game_phase(board);
  if (phase < kPhaseMax / 8) {
    return;
  }

  const Color them = !c;
  const Bitboard zone = space_zone_mask(c);
  const Bitboard our_pawn_attacks = pawn_attack_mask(board, c);
  const Bitboard their_pawn_attacks = pawn_attack_mask(board, them);

  Bitboard scan = zone;
  while (scan) {
    const Square sq = bb::pop_lsb(scan);
    if (our_pawn_attacks & bb::square_bb(sq)) {
      if ((their_pawn_attacks & bb::square_bb(sq)) == 0) {
        mg += kSafeSquareMg;
      }
    }
  }

  Bitboard pawns = board.pieces(c, PieceType::Pawn);
  while (pawns) {
    const Square sq = bb::pop_lsb(pawns);
    const int file = bb::file_of(sq);
    if (file == 3 || file == 4) {
      mg += kCentralPawnMg;
      eg += kCentralPawnEg;
    }
  }

  const int scaled = (mg * phase) / kPhaseMax;
  mg = scaled;
  eg = (eg * phase) / kPhaseMax;
}

}  // namespace

void evaluate_space_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                          int& eg_black) {
  space_side(board, Color::White, mg_white, eg_white);
  space_side(board, Color::Black, mg_black, eg_black);
}

}  // namespace engine::search_common
