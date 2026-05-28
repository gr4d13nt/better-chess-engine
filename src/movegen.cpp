#include "engine/movegen.hpp"

#include "engine/attacks.hpp"

#include <string>

namespace engine {
namespace {

constexpr std::uint8_t kCastleWK = 1;
constexpr std::uint8_t kCastleWQ = 2;
constexpr std::uint8_t kCastleBK = 4;
constexpr std::uint8_t kCastleBQ = 8;

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

bool promotion_rank(Color us, Square sq) {
  const int r = bb::rank_of(sq);
  return (us == Color::White && r == 7) || (us == Color::Black && r == 0);
}

void add_promotions(MoveList& list, Square from, Square to, bool capture) {
  for (PieceType pt :
       {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
    Move m(from, to, capture ? MoveFlag::PromotionCapture : MoveFlag::Promotion);
    m.promotion = pt;
    list.add(m);
  }
}

bool blocked_by_pin(Bitboard pinned_bb, Square from) {
  return pinned_bb & bb::square_bb(from);
}

void generate_pawns(const Board& board, Color us, Bitboard pin_ray, Bitboard pinned_bb, MoveList& list) {
  const Color them = !us;
  const Bitboard enemy = board.color_bb(them);
  const Bitboard occ = board.occupied();
  Bitboard pawns = board.pieces(us, PieceType::Pawn);
  const int push = us == Color::White ? 8 : -8;
  const int push2 = push * 2;

  while (pawns) {
    const Square from = bb::pop_lsb(pawns);
    const int from_i = static_cast<int>(from);
    const int file = bb::file_of(from);
    const Square to = static_cast<Square>(from_i + push);

    if (!(occ & bb::square_bb(to))) {
      if (!blocked_by_pin(pinned_bb, from) || (pin_ray & bb::square_bb(to))) {
        if (promotion_rank(us, to)) {
          add_promotions(list, from, to, false);
        } else {
          list.add(Move(from, to, MoveFlag::Quiet));
          const bool start_rank = (us == Color::White && bb::rank_of(from) == 1) ||
                                  (us == Color::Black && bb::rank_of(from) == 6);
          if (start_rank) {
            const Square mid = static_cast<Square>(from_i + push);
            const Square to2 = static_cast<Square>(from_i + push2);
            if (!(occ & bb::square_bb(mid)) && !(occ & bb::square_bb(to2))) {
              list.add(Move(from, to2, MoveFlag::DoublePush));
            }
          }
        }
      }
    }

    const int cap_deltas[2] = {
        us == Color::White ? 7 : -9,
        us == Color::White ? 9 : -7,
    };
    const bool can_cap[2] = {file > 0, file < 7};
    for (int i = 0; i < 2; ++i) {
      if (!can_cap[i]) {
        continue;
      }
      const Square cap_to = static_cast<Square>(from_i + cap_deltas[i]);
      if (!(enemy & bb::square_bb(cap_to))) {
        continue;
      }
      if (blocked_by_pin(pinned_bb, from) && !(pin_ray & bb::square_bb(cap_to))) {
        continue;
      }
      if (promotion_rank(us, cap_to)) {
        add_promotions(list, from, cap_to, true);
      } else {
        list.add(Move(from, cap_to, MoveFlag::Capture));
      }
    }
  }

  if (board.ep_square() != Square::None) {
    const Square ep = board.ep_square();
    const int ep_i = static_cast<int>(ep);
    const int ep_file = ep_i & 7;
    const int ep_rank = ep_i >> 3;
  const int from_rank = us == Color::White ? ep_rank - 1 : ep_rank + 1;
    if (from_rank >= 0 && from_rank < 8) {
      if (ep_file > 0) {
        const Square from = static_cast<Square>(from_rank * 8 + ep_file - 1);
        if (board.pieces(us, PieceType::Pawn) & bb::square_bb(from)) {
          if (!blocked_by_pin(pinned_bb, from)) {
            list.add(Move(from, ep, MoveFlag::EnPassant));
          }
        }
      }
      if (ep_file < 7) {
        const Square from = static_cast<Square>(from_rank * 8 + ep_file + 1);
        if (board.pieces(us, PieceType::Pawn) & bb::square_bb(from)) {
          if (!blocked_by_pin(pinned_bb, from)) {
            list.add(Move(from, ep, MoveFlag::EnPassant));
          }
        }
      }
    }
  }
}

template <typename AttackFn>
void generate_slider(const Board& board, Color us, Bitboard pieces, Bitboard pin_ray, Bitboard pinned_bb,
                     AttackFn attack_fn, MoveList& list) {
  const Bitboard enemy = board.color_bb(!us);
  const Bitboard occ = board.occupied();
  while (pieces) {
    const Square from = bb::pop_lsb(pieces);
    Bitboard targets = attack_fn(from, occ) & ~board.color_bb(us);
    if (blocked_by_pin(pinned_bb, from)) {
      targets &= pin_ray;
    }
    while (targets) {
      const Square to = bb::pop_lsb(targets);
      list.add(Move(from, to, (enemy & bb::square_bb(to)) ? MoveFlag::Capture : MoveFlag::Quiet));
    }
  }
}

void generate_knights(const Board& board, Color us, Bitboard pinned_bb, MoveList& list) {
  Bitboard knights = board.pieces(us, PieceType::Knight);
  const Bitboard enemy = board.color_bb(!us);
  while (knights) {
    const Square from = bb::pop_lsb(knights);
    if (blocked_by_pin(pinned_bb, from)) {
      continue;
    }
    Bitboard targets = knight_attacks(from) & ~board.color_bb(us);
    while (targets) {
      const Square to = bb::pop_lsb(targets);
      list.add(Move(from, to, (enemy & bb::square_bb(to)) ? MoveFlag::Capture : MoveFlag::Quiet));
    }
  }
}

void generate_king(const Board& board, Color us, MoveList& list) {
  const Bitboard kings = board.pieces(us, PieceType::King);
  const Square from = static_cast<Square>(bb::lsb_index(kings));
  const Bitboard enemy = board.color_bb(!us);
  Bitboard targets = king_attacks(from) & ~board.color_bb(us);
  while (targets) {
    const Square to = bb::pop_lsb(targets);
    list.add(Move(from, to, (enemy & bb::square_bb(to)) ? MoveFlag::Capture : MoveFlag::Quiet));
  }
}

bool square_attacked(const Board& board, Square sq, Color by) {
  return attackers_to(board, sq, by) != 0;
}

void generate_castling(Board& board, Color us, Bitboard checkers, MoveList& list) {
  if (checkers) {
    return;
  }
  const std::uint8_t rights = board.castling_rights();
  const Bitboard occ = board.occupied();
  const Color them = !us;
  const Bitboard rooks = board.pieces(us, PieceType::Rook);

  if (us == Color::White) {
    if ((rights & kCastleWK) && (rooks & bb::square_bb(Square::H1)) &&
        !(occ & (bb::square_bb(Square::F1) | bb::square_bb(Square::G1)))) {
      if (!square_attacked(board, Square::E1, them) && !square_attacked(board, Square::F1, them) &&
          !square_attacked(board, Square::G1, them)) {
        list.add(Move(Square::E1, Square::G1, MoveFlag::Castle));
      }
    }
    if ((rights & kCastleWQ) && (rooks & bb::square_bb(Square::A1)) &&
        !(occ & (bb::square_bb(Square::D1) | bb::square_bb(Square::C1) | bb::square_bb(Square::B1)))) {
      if (!square_attacked(board, Square::E1, them) && !square_attacked(board, Square::D1, them) &&
          !square_attacked(board, Square::C1, them)) {
        list.add(Move(Square::E1, Square::C1, MoveFlag::Castle));
      }
    }
  } else {
    if ((rights & kCastleBK) && (rooks & bb::square_bb(Square::H8)) &&
        !(occ & (bb::square_bb(Square::F8) | bb::square_bb(Square::G8)))) {
      if (!square_attacked(board, Square::E8, them) && !square_attacked(board, Square::F8, them) &&
          !square_attacked(board, Square::G8, them)) {
        list.add(Move(Square::E8, Square::G8, MoveFlag::Castle));
      }
    }
    if ((rights & kCastleBQ) && (rooks & bb::square_bb(Square::A8)) &&
        !(occ & (bb::square_bb(Square::D8) | bb::square_bb(Square::C8) | bb::square_bb(Square::B8)))) {
      if (!square_attacked(board, Square::E8, them) && !square_attacked(board, Square::D8, them) &&
          !square_attacked(board, Square::C8, them)) {
        list.add(Move(Square::E8, Square::C8, MoveFlag::Castle));
      }
    }
  }
}

void generate_pseudo_legal(Board& board, Color us, Bitboard pin_ray, Bitboard pinned_bb, Square king_sq,
                           Bitboard checkers, MoveList& pseudo) {
  if (bb::popcount(checkers) > 1) {
    generate_king(board, us, pseudo);
    return;
  }

  Bitboard check_mask = ~0ULL;
  if (checkers) {
    const Square checker = static_cast<Square>(bb::lsb_index(checkers));
    check_mask = between_squares(king_sq, checker) | bb::square_bb(checker);
  }

  generate_pawns(board, us, pin_ray, pinned_bb, pseudo);
  generate_knights(board, us, pinned_bb, pseudo);

  generate_slider(
      board, us, board.pieces(us, PieceType::Bishop), pin_ray, pinned_bb,
      [](Square sq, Bitboard occ) { return bishop_attacks(sq, occ); }, pseudo);
  generate_slider(
      board, us, board.pieces(us, PieceType::Rook), pin_ray, pinned_bb,
      [](Square sq, Bitboard occ) { return rook_attacks(sq, occ); }, pseudo);
  generate_slider(
      board, us, board.pieces(us, PieceType::Queen), pin_ray, pinned_bb,
      [](Square sq, Bitboard occ) { return queen_attacks(sq, occ); }, pseudo);

  generate_king(board, us, pseudo);
  generate_castling(board, us, checkers, pseudo);

  if (!checkers) {
    return;
  }

  MoveList filtered;
  filtered.count = 0;
  for (int i = 0; i < pseudo.count; ++i) {
    const Move& m = pseudo.moves[i];
    const Piece pf = board.piece_on(m.from);
    if (piece_type(pf) == PieceType::King || (bb::square_bb(m.to) & check_mask) ||
        m.flag == MoveFlag::EnPassant || m.flag == MoveFlag::Castle) {
      filtered.add(m);
    }
  }
  pseudo.count = filtered.count;
  for (int i = 0; i < filtered.count; ++i) {
    pseudo.moves[i] = filtered.moves[i];
  }
}

}  // namespace

Bitboard attackers_to(const Board& board, Square sq, Color by_color) {
  const Bitboard occ = board.occupied();
  Bitboard attackers = 0;

  attackers |= pawn_attacks(!by_color, sq) & board.pieces(by_color, PieceType::Pawn);
  attackers |= knight_attacks(sq) & board.pieces(by_color, PieceType::Knight);
  attackers |= king_attacks(sq) & board.pieces(by_color, PieceType::King);

  Bitboard bishops = board.pieces(by_color, PieceType::Bishop) | board.pieces(by_color, PieceType::Queen);
  while (bishops) {
    const Square from = bb::pop_lsb(bishops);
    if (bishop_attacks(from, occ) & bb::square_bb(sq)) {
      attackers |= bb::square_bb(from);
    }
  }

  Bitboard rooks = board.pieces(by_color, PieceType::Rook) | board.pieces(by_color, PieceType::Queen);
  while (rooks) {
    const Square from = bb::pop_lsb(rooks);
    if (rook_attacks(from, occ) & bb::square_bb(sq)) {
      attackers |= bb::square_bb(from);
    }
  }

  return attackers;
}

void generate_legal_moves(const Board& board_in, MoveList& out) {
  out.clear();
  Board board = board_in;

  const Color us = board.side_to_move();
  const Color them = !us;
  const Square king_sq =
      static_cast<Square>(bb::lsb_index(board.pieces(us, PieceType::King)));

  const Bitboard checkers = attackers_to(board, king_sq, them) & board.color_bb(them);

  Bitboard pin_ray = 0;
  Bitboard pinned_bb = 0;
  Bitboard bishops = board.pieces(them, PieceType::Bishop) | board.pieces(them, PieceType::Queen);
  Bitboard rooks = board.pieces(them, PieceType::Rook) | board.pieces(them, PieceType::Queen);

  auto add_pins = [&](Bitboard pieces, bool bishop) {
    while (pieces) {
      const Square sniper = bb::pop_lsb(pieces);
      const Bitboard occ = board.occupied();
      Bitboard attacks = bishop ? bishop_attacks(sniper, occ) : rook_attacks(sniper, occ);
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

  add_pins(bishops, true);
  add_pins(rooks, false);

  MoveList pseudo;
  generate_pseudo_legal(board, us, pin_ray, pinned_bb, king_sq, checkers, pseudo);

  for (int i = 0; i < pseudo.count; ++i) {
    const Move& m = pseudo.moves[i];
    Undo undo;
    board.make_move(m, undo);
    if (!board.in_check(us)) {
      out.add(m);
    }
    board.unmake_move(m, undo);
  }
}

std::string move_to_uci(const Move& m) {
  auto sq_char = [](Square sq) {
    return std::string{static_cast<char>('a' + bb::file_of(sq)),
                       static_cast<char>('1' + bb::rank_of(sq))};
  };
  std::string s = sq_char(m.from) + sq_char(m.to);
  if (m.promotion != PieceType::None) {
  switch (m.promotion) {
      case PieceType::Knight: s += 'n'; break;
      case PieceType::Bishop: s += 'b'; break;
      case PieceType::Rook: s += 'r'; break;
      case PieceType::Queen: s += 'q'; break;
      default: break;
    }
  }
  return s;
}

}  // namespace engine
