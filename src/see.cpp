#include "engine/see.hpp"

#include "engine/attacks.hpp"
#include "engine/movegen.hpp"

#include <algorithm>

namespace engine {
namespace {

constexpr int kPieceValues[kNumPieceTypes] = {
    100,  // Pawn
    320,  // Knight
    330,  // Bishop
    500,  // Rook
    900,  // Queen
    0,    // King
};

int value_of(PieceType pt) { return kPieceValues[static_cast<int>(pt)]; }

Bitboard between_squares(Square a, Square b) {
  if (a == b) {
    return 0;
  }
  const Bitboard occ = bb::square_bb(b);
  if ((bishop_attacks(a, occ) & bb::square_bb(b)) && (bishop_attacks(b, occ) & bb::square_bb(a))) {
    return bishop_attacks(a, 0) & bishop_attacks(b, 0) & ~bb::square_bb(a) & ~bb::square_bb(b);
  }
  if ((rook_attacks(a, occ) & bb::square_bb(b)) && (rook_attacks(b, occ) & bb::square_bb(a))) {
    return rook_attacks(a, 0) & rook_attacks(b, 0) & ~bb::square_bb(a) & ~bb::square_bb(b);
  }
  return 0;
}

void pin_masks_for(const Board& board, Color us, Bitboard& pin_ray, Bitboard& pinned_bb) {
  pin_ray = 0;
  pinned_bb = 0;

  const Square king_sq =
      static_cast<Square>(bb::lsb_index(board.pieces(us, PieceType::King)));
  const Color them = !us;

  auto add_pins = [&](Bitboard pieces, bool bishop) {
    while (pieces) {
      const Square sniper = bb::pop_lsb(pieces);
      const Bitboard occ = board.occupied();
      const Bitboard attacks =
          bishop ? bishop_attacks(sniper, occ) : rook_attacks(sniper, occ);
      if (!(attacks & bb::square_bb(king_sq))) {
        continue;
      }
      const Bitboard between = between_squares(king_sq, sniper);
      if (bb::popcount(between & board.color_bb(us)) == 1) {
        pin_ray |= between | bb::square_bb(sniper);
        pinned_bb |= between & board.color_bb(us);
      }
    }
  };

  const Bitboard bishops =
      board.pieces(them, PieceType::Bishop) | board.pieces(them, PieceType::Queen);
  const Bitboard rooks =
      board.pieces(them, PieceType::Rook) | board.pieces(them, PieceType::Queen);
  add_pins(bishops, true);
  add_pins(rooks, false);
}

Bitboard legal_recapture_attackers(const Board& board, Square to, Color side, Bitboard occupied) {
  Bitboard attackers = attackers_to(board, to, side) & occupied;

  Bitboard pin_ray = 0;
  Bitboard pinned_bb = 0;
  pin_masks_for(board, side, pin_ray, pinned_bb);

  Bitboard legal = 0;
  while (attackers) {
    const Square sq = bb::pop_lsb(attackers);
    if (pinned_bb & bb::square_bb(sq)) {
      if (pin_ray & bb::square_bb(to)) {
        legal |= bb::square_bb(sq);
      }
    } else {
      legal |= bb::square_bb(sq);
    }
  }
  return legal;
}

Square ep_capture_square(const Board& board, const Move& m) {
  const int to_rank = bb::rank_of(m.to);
  if (board.side_to_move() == Color::White) {
    return bb::make_square(bb::file_of(m.to), to_rank - 1);
  }
  return bb::make_square(bb::file_of(m.to), to_rank + 1);
}

Square victim_square(const Board& board, const Move& m) {
  if (m.flag == MoveFlag::EnPassant) {
    return ep_capture_square(board, m);
  }
  return m.to;
}

bool find_least_valuable_attacker(const Board& board, Bitboard attackers, Color side,
                                  Square& out_sq, PieceType& out_pt) {
  for (int pt = static_cast<int>(PieceType::Pawn); pt <= static_cast<int>(PieceType::King); ++pt) {
    const Bitboard pieces = attackers & board.pieces(side, static_cast<PieceType>(pt));
    if (pieces) {
      out_sq = static_cast<Square>(bb::lsb_index(pieces));
      out_pt = static_cast<PieceType>(pt);
      return true;
    }
  }
  return false;
}

int see_recapture(const Board& board, Square to, Bitboard occupied, Color side) {
  const Bitboard attackers = legal_recapture_attackers(board, to, side, occupied);
  if (attackers == 0) {
    return 0;
  }

  Square attacker_sq = Square::None;
  PieceType attacker_pt = PieceType::None;
  if (!find_least_valuable_attacker(board, attackers, side, attacker_sq, attacker_pt)) {
    return 0;
  }
  if (attacker_pt == PieceType::King) {
    return 0;
  }

  occupied ^= bb::square_bb(attacker_sq);
  const int gain = value_of(attacker_pt) - see_recapture(board, to, occupied, !side);
  return std::max(0, gain);
}

}  // namespace

int see_capture(const Board& board, const Move& m) {
  if (m.flag != MoveFlag::Capture && m.flag != MoveFlag::PromotionCapture &&
      m.flag != MoveFlag::EnPassant) {
    return 0;
  }

  int captured = 0;
  if (m.flag == MoveFlag::EnPassant) {
    captured = value_of(PieceType::Pawn);
  } else {
    const Piece victim = board.piece_on(m.to);
    if (victim == Piece::None) {
      return 0;
    }
    captured = value_of(piece_type(victim));
  }

  const Square victim_sq = victim_square(board, m);
  Bitboard occupied = board.occupied();
  occupied &= ~bb::square_bb(m.from);
  occupied &= ~bb::square_bb(victim_sq);
  occupied |= bb::square_bb(m.to);

  return captured - see_recapture(board, m.to, occupied, !board.side_to_move());
}

}  // namespace engine
