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

void add_pawn_moves_from_targets(Bitboard targets, int from_delta, MoveList& list, MoveFlag flag) {
  while (targets) {
    const Square to = bb::pop_lsb(targets);
    const Square from = static_cast<Square>(static_cast<int>(to) + from_delta);
    list.add(Move(from, to, flag));
  }
}

void add_pawn_push_moves(Color us, Bitboard quiet_targets, Bitboard promo_targets, MoveList& list) {
  const int push_delta = us == Color::White ? -8 : 8;
  add_pawn_moves_from_targets(quiet_targets, push_delta, list, MoveFlag::Quiet);
  while (promo_targets) {
    const Square to = bb::pop_lsb(promo_targets);
    const Square from = static_cast<Square>(static_cast<int>(to) + push_delta);
    add_promotions(list, from, to, false);
  }
}

void add_pawn_capture_moves(Bitboard quiet_targets, Bitboard promo_targets, int from_delta, MoveList& list) {
  add_pawn_moves_from_targets(quiet_targets, from_delta, list, MoveFlag::Capture);
  while (promo_targets) {
    const Square to = bb::pop_lsb(promo_targets);
    const Square from = static_cast<Square>(static_cast<int>(to) + from_delta);
    add_promotions(list, from, to, true);
  }
}

void generate_pawn_bulk(Color us, Bitboard pawns, Bitboard occ, Bitboard enemy, Bitboard allowed_targets,
                        MoveList& list) {
  if (!pawns) {
    return;
  }

  if (us == Color::White) {
    const Bitboard push_one = bb::north(pawns) & ~occ & allowed_targets;
    add_pawn_push_moves(us, push_one & ~bb::kRank8, push_one & bb::kRank8, list);

    const Bitboard double_targets =
        bb::north(bb::north(pawns & bb::kRank2) & ~occ) & ~occ & allowed_targets;
    add_pawn_moves_from_targets(double_targets, -16, list, MoveFlag::DoublePush);

    const Bitboard cap_left = bb::north_west(pawns) & enemy & allowed_targets;
    add_pawn_capture_moves(cap_left & ~bb::kRank8, cap_left & bb::kRank8, -7, list);

    const Bitboard cap_right = bb::north_east(pawns) & enemy & allowed_targets;
    add_pawn_capture_moves(cap_right & ~bb::kRank8, cap_right & bb::kRank8, -9, list);
  } else {
    const Bitboard push_one = bb::south(pawns) & ~occ & allowed_targets;
    add_pawn_push_moves(us, push_one & ~bb::kRank1, push_one & bb::kRank1, list);

    const Bitboard double_targets =
        bb::south(bb::south(pawns & bb::kRank7) & ~occ) & ~occ & allowed_targets;
    add_pawn_moves_from_targets(double_targets, 16, list, MoveFlag::DoublePush);

    const Bitboard cap_left = bb::south_east(pawns) & enemy & allowed_targets;
    add_pawn_capture_moves(cap_left & ~bb::kRank1, cap_left & bb::kRank1, 7, list);

    const Bitboard cap_right = bb::south_west(pawns) & enemy & allowed_targets;
    add_pawn_capture_moves(cap_right & ~bb::kRank1, cap_right & bb::kRank1, 9, list);
  }
}

void generate_pawns(const Board& board, Color us, Bitboard pin_ray, Bitboard pinned_bb, MoveList& list) {
  const Color them = !us;
  const Bitboard enemy = board.color_bb(them);
  const Bitboard occ = board.occupied();
  const Bitboard pawns = board.pieces(us, PieceType::Pawn);

  generate_pawn_bulk(us, pawns & ~pinned_bb, occ, enemy, ~0ULL, list);
  generate_pawn_bulk(us, pawns & pinned_bb, occ, enemy, pin_ray, list);

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
          if (!(pinned_bb & bb::square_bb(from))) {
            list.add(Move(from, ep, MoveFlag::EnPassant));
          }
        }
      }
      if (ep_file < 7) {
        const Square from = static_cast<Square>(from_rank * 8 + ep_file + 1);
        if (board.pieces(us, PieceType::Pawn) & bb::square_bb(from)) {
          if (!(pinned_bb & bb::square_bb(from))) {
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

Bitboard attackers_to_occ(const Board& board, Square sq, Color by_color, Bitboard occ,
                          Bitboard removed_enemy) {
  Bitboard attackers = 0;

  attackers |= pawn_attacks(!by_color, sq) & board.pieces(by_color, PieceType::Pawn) & ~removed_enemy;
  attackers |= knight_attacks(sq) & board.pieces(by_color, PieceType::Knight) & ~removed_enemy;
  attackers |= king_attacks(sq) & board.pieces(by_color, PieceType::King) & ~removed_enemy;

  Bitboard bishops =
      (board.pieces(by_color, PieceType::Bishop) | board.pieces(by_color, PieceType::Queen)) & ~removed_enemy;
  while (bishops) {
    const Square from = bb::pop_lsb(bishops);
    if (bishop_attacks(from, occ) & bb::square_bb(sq)) {
      attackers |= bb::square_bb(from);
    }
  }

  Bitboard rooks =
      (board.pieces(by_color, PieceType::Rook) | board.pieces(by_color, PieceType::Queen)) & ~removed_enemy;
  while (rooks) {
    const Square from = bb::pop_lsb(rooks);
    if (rook_attacks(from, occ) & bb::square_bb(sq)) {
      attackers |= bb::square_bb(from);
    }
  }

  return attackers;
}

bool leaves_king_in_check(const Board& board, Color us, Color them, Square king_sq, const Move& move) {
  if (move.flag == MoveFlag::Castle) {
    return false;
  }

  Bitboard occ = board.occupied();
  const Bitboard from_bb = bb::square_bb(move.from);
  const Bitboard to_bb = bb::square_bb(move.to);
  Bitboard removed_enemy = 0;

  if (move.flag == MoveFlag::EnPassant) {
    const Square cap_sq = us == Color::White ? bb::shift_down(move.to) : bb::shift_up(move.to);
    removed_enemy = bb::square_bb(cap_sq);
    occ = (occ & ~from_bb & ~removed_enemy) | to_bb;
  } else {
    if (occ & to_bb & board.color_bb(them)) {
      removed_enemy = to_bb;
    }
    occ = (occ & ~from_bb) | to_bb;
  }

  const Square king_sq_after = move.from == king_sq ? move.to : king_sq;
  return attackers_to_occ(board, king_sq_after, them, occ, removed_enemy) != 0;
}

void generate_castling(const Board& board, Color us, Bitboard checkers, MoveList& list) {
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

void generate_pseudo_legal(const Board& board, Color us, Bitboard pin_ray, Bitboard pinned_bb, Square king_sq,
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
  return attackers_to_occ(board, sq, by_color, board.occupied(), 0);
}

void generate_legal_moves(const Board& board, MoveList& out) {
  out.clear();

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
    if (!leaves_king_in_check(board, us, them, king_sq, m)) {
      out.add(m);
    }
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
