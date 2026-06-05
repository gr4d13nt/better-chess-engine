#include "search_eval_hanging.hpp"

#include "engine/movegen.hpp"
#include "search_eval_phase.hpp"

namespace engine::search_common {
namespace {

constexpr int kPieceValue[6] = {100, 320, 330, 500, 900, 0};

int piece_value(PieceType pt) {
  const int idx = static_cast<int>(pt);
  if (idx < 0 || idx >= 6) {
    return 0;
  }
  return kPieceValue[idx];
}

void hanging_side(const Board& board, Color c, int& mg, int& eg) {
  mg = 0;
  eg = 0;

  const Color them = !c;
  const Bitboard their_pieces = board.color_bb(them);

  for (int pt = static_cast<int>(PieceType::Knight); pt <= static_cast<int>(PieceType::Queen);
       ++pt) {
    const PieceType piece_type = static_cast<PieceType>(pt);
    const int value = piece_value(piece_type);
    Bitboard pieces = board.pieces(c, piece_type);
    while (pieces) {
      const Square sq = bb::pop_lsb(pieces);
      const Bitboard them_att =
          attackers_to(board, sq, them) & their_pieces;
      if (them_att == 0) {
        continue;
      }
      const Bitboard us_att = attackers_to(board, sq, c) & board.color_bb(c);
      if (us_att == 0) {
        mg -= (value * 65) / 100;
        eg -= (value * 25) / 100;
      } else if (bb::popcount(them_att) > bb::popcount(us_att)) {
        mg -= (value * 28) / 100;
        eg -= (value * 12) / 100;
      }
    }
  }

  const int phase = game_phase(board);
  mg = (mg * phase) / kPhaseMax;
  eg = (eg * phase) / kPhaseMax;
}

}  // namespace

void evaluate_hanging_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                            int& eg_black) {
  hanging_side(board, Color::White, mg_white, eg_white);
  hanging_side(board, Color::Black, mg_black, eg_black);
}

}  // namespace engine::search_common
