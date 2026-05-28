#pragma once

#include "engine/bitboard.hpp"
#include "engine/move.hpp"
#include "engine/types.hpp"

#include <array>
#include <string>

namespace engine {

struct Undo {
  Move move;
  Piece captured_piece = Piece::None;
  PieceType moved_piece = PieceType::None;
  Square ep_square = Square::None;
  std::uint8_t castling_rights = 0;
  int halfmove_clock = 0;
  Bitboard captured_bb = 0;
};

class Board {
 public:
  Board();

  void set_startpos();
  bool set_fen(const std::string& fen);

  Color side_to_move() const { return side_; }
  Square ep_square() const { return ep_square_; }
  std::uint8_t castling_rights() const { return castling_rights_; }

  Bitboard pieces(Color c, PieceType pt) const { return pieces_[static_cast<int>(c)][static_cast<int>(pt)]; }
  Bitboard color_bb(Color c) const { return color_bb_[static_cast<int>(c)]; }
  Bitboard occupied() const { return occupied_; }

  Piece piece_on(Square sq) const;
  bool square_occupied(Square sq) const { return occupied_ & bb::square_bb(sq); }

  bool in_check(Color c) const;
  bool is_checkmate() const;
  bool is_stalemate() const;

  void make_move(const Move& move, Undo& undo);
  void unmake_move(const Move& move, const Undo& undo);

  std::string fen() const;

 private:
  void clear();
  void refresh_derived();
  void update_occupied(Color c, PieceType pt, Square sq, bool add);

  std::array<std::array<Bitboard, kNumPieceTypes>, kNumColors> pieces_{};
  std::array<Bitboard, kNumColors> color_bb_{};
  Bitboard occupied_ = 0;

  Color side_ = Color::White;
  Square ep_square_ = Square::None;
  std::uint8_t castling_rights_ = 0;
  int halfmove_clock_ = 0;
  int fullmove_number_ = 1;
};

}  // namespace engine
