#include "search_eval_space.hpp"

#include "search_eval_phase.hpp"

namespace engine::search_common {
namespace {

constexpr Bitboard file_bb(int file) { return bb::kFileA << file; }

constexpr Bitboard rank_bb(int rank) { return bb::kRank1 << (rank * 8); }

constexpr int kSafeSquareMg = 4;
constexpr int kCentralPawnMg = 8;
constexpr int kCentralPawnEg = 2;

Bitboard pawn_attack_mask(Color c, Bitboard pawns) {
  if (c == Color::White) {
    return bb::north_west(pawns) | bb::north_east(pawns);
  }
  return bb::south_west(pawns) | bb::south_east(pawns);
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
  const Bitboard pawns = board.pieces(c, PieceType::Pawn);
  const Bitboard our_pawn_attacks = pawn_attack_mask(c, pawns);
  const Bitboard their_pawn_attacks = pawn_attack_mask(them, board.pieces(them, PieceType::Pawn));
  const Bitboard safe_squares = zone & our_pawn_attacks & ~their_pawn_attacks;

  mg += kSafeSquareMg * bb::popcount(safe_squares);

  const Bitboard central_pawns = pawns & (file_bb(3) | file_bb(4));
  const int central_count = bb::popcount(central_pawns);
  mg += kCentralPawnMg * central_count;
  eg += kCentralPawnEg * central_count;

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
