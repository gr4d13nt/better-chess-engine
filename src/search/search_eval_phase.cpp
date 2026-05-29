#include "search_eval_phase.hpp"

namespace engine::search_common {

namespace {

constexpr int kKnightPhase = 1;
constexpr int kBishopPhase = 1;
constexpr int kRookPhase = 2;
constexpr int kQueenPhase = 4;

int phase_units_for(PieceType pt) {
  switch (pt) {
    case PieceType::Knight:
      return kKnightPhase;
    case PieceType::Bishop:
      return kBishopPhase;
    case PieceType::Rook:
      return kRookPhase;
    case PieceType::Queen:
      return kQueenPhase;
    default:
      return 0;
  }
}

int phase_units_on_board(const Board& board) {
  int units = 0;
  for (int c = 0; c < kNumColors; ++c) {
    for (int pt = static_cast<int>(PieceType::Knight); pt <= static_cast<int>(PieceType::Queen); ++pt) {
      units += bb::popcount(board.pieces(static_cast<Color>(c), static_cast<PieceType>(pt))) *
                phase_units_for(static_cast<PieceType>(pt));
    }
  }
  return units;
}

}  // namespace

int game_phase(const Board& board) {
  const int max_units = 2 * (2 * kKnightPhase + 2 * kBishopPhase + 2 * kRookPhase + 2 * kQueenPhase);
  int units = phase_units_on_board(board);

  // Pawn-only positions still need a meaningful blend (e.g. structure tests / pawn endings).
  if (units == 0) {
    units = bb::popcount(board.pieces(Color::White, PieceType::Pawn)) +
            bb::popcount(board.pieces(Color::Black, PieceType::Pawn));
    const int max_pawn_units = 16;
    const int pawn_phase = (units * kPhaseMax) / max_pawn_units;
    return pawn_phase;
  }

  const int phase = (units * kPhaseMax) / max_units;
  if (phase < 0) {
    return 0;
  }
  if (phase > kPhaseMax) {
    return kPhaseMax;
  }
  return phase;
}

int taper_eval(int mg, int eg, int phase) {
  if (phase <= 0) {
    return eg;
  }
  if (phase >= kPhaseMax) {
    return mg;
  }
  return (mg * phase + eg * (kPhaseMax - phase)) / kPhaseMax;
}

}  // namespace engine::search_common
