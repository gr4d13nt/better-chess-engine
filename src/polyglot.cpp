#include "engine/polyglot.hpp"

#include "engine/bitboard.hpp"
#include "engine/movegen.hpp"
#include "engine/polyglot_keys.hpp"

namespace engine {
namespace {

constexpr std::uint8_t kCastleWK = 1;
constexpr std::uint8_t kCastleWQ = 2;
constexpr std::uint8_t kCastleBK = 4;
constexpr std::uint8_t kCastleBQ = 8;

int polyglot_piece_index(Color color, PieceType pt) {
  // Polyglot order: black pawn, white pawn, black knight, white knight, ...
  const int polyglot_color = color == Color::White ? 1 : 0;
  return static_cast<int>(pt) * 2 + polyglot_color;
}

bool ep_has_capturer(const Board& board) {
  const Square ep = board.ep_square();
  if (ep == Square::None) {
    return false;
  }

  const Color us = board.side_to_move();
  const int ep_file = bb::file_of(ep);
  const int ep_rank = bb::rank_of(ep);
  const Bitboard pawns = board.pieces(us, PieceType::Pawn);

  if (us == Color::White) {
    if (ep_rank != 5) {
      return false;
    }
    for (int df : {-1, 1}) {
      const int f = ep_file + df;
      if (f < 0 || f > 7) {
        continue;
      }
      const Square sq = static_cast<Square>((ep_rank - 1) * 8 + f);
      if (pawns & bb::square_bb(sq)) {
        return true;
      }
    }
    return false;
  }

  if (ep_rank != 2) {
    return false;
  }
  for (int df : {-1, 1}) {
    const int f = ep_file + df;
    if (f < 0 || f > 7) {
      continue;
    }
    const Square sq = static_cast<Square>((ep_rank + 1) * 8 + f);
    if (pawns & bb::square_bb(sq)) {
      return true;
    }
  }
  return false;
}

PieceType polyglot_promo_type(int promo_part) {
  switch (promo_part) {
    case 1:
      return PieceType::Knight;
    case 2:
      return PieceType::Bishop;
    case 3:
      return PieceType::Rook;
    case 4:
      return PieceType::Queen;
    default:
      return PieceType::None;
  }
}

bool move_matches_polyglot(const Move& move, Square from, Square to, PieceType promo) {
  if (move.from != from || move.to != to) {
    return false;
  }
  if (promo == PieceType::None) {
    return move.promotion == PieceType::None;
  }
  return move.promotion == promo;
}

}  // namespace

std::uint64_t polyglot_hash(const Board& board) {
  using polyglot_keys::kRandom;

  std::uint64_t key = 0;
  for (int c = 0; c < kNumColors; ++c) {
    for (int pt = 0; pt < kNumPieceTypes; ++pt) {
      Bitboard bb = board.pieces(static_cast<Color>(c), static_cast<PieceType>(pt));
      while (bb) {
        const Square sq = bb::pop_lsb(bb);
        const int piece_index = polyglot_piece_index(static_cast<Color>(c), static_cast<PieceType>(pt));
        key ^= kRandom[64 * piece_index + square_index(sq)];
      }
    }
  }

  const std::uint8_t castle = board.castling_rights();
  if (castle & kCastleWK) {
    key ^= kRandom[768];
  }
  if (castle & kCastleWQ) {
    key ^= kRandom[769];
  }
  if (castle & kCastleBK) {
    key ^= kRandom[770];
  }
  if (castle & kCastleBQ) {
    key ^= kRandom[771];
  }

  if (ep_has_capturer(board)) {
    key ^= kRandom[772 + bb::file_of(board.ep_square())];
  }

  if (board.side_to_move() == Color::White) {
    key ^= kRandom[780];
  }

  return key;
}

std::optional<Move> polyglot_move_to_engine(const Board& board, std::uint16_t raw_move) {
  const Square from = static_cast<Square>((raw_move >> 6) & 0x3F);
  const Square to = static_cast<Square>(raw_move & 0x3F);
  const PieceType promo = polyglot_promo_type((raw_move >> 12) & 0x7);

  MoveList moves;
  generate_legal_moves(board, moves);
  for (int i = 0; i < moves.count; ++i) {
    if (move_matches_polyglot(moves.moves[i], from, to, promo)) {
      return moves.moves[i];
    }
  }
  return std::nullopt;
}

}  // namespace engine
