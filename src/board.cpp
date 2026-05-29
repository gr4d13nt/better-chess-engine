#include "engine/board.hpp"

#include "engine/attacks.hpp"
#include "engine/movegen.hpp"
#include "engine/zobrist.hpp"

#include <sstream>
#include <stdexcept>

namespace engine {
namespace {

constexpr std::uint8_t kCastleWK = 1;
constexpr std::uint8_t kCastleWQ = 2;
constexpr std::uint8_t kCastleBK = 4;
constexpr std::uint8_t kCastleBQ = 8;

char piece_to_char(Piece p) {
  switch (p) {
    case Piece::W_Pawn: return 'P';
    case Piece::W_Knight: return 'N';
    case Piece::W_Bishop: return 'B';
    case Piece::W_Rook: return 'R';
    case Piece::W_Queen: return 'Q';
    case Piece::W_King: return 'K';
    case Piece::B_Pawn: return 'p';
    case Piece::B_Knight: return 'n';
    case Piece::B_Bishop: return 'b';
    case Piece::B_Rook: return 'r';
    case Piece::B_Queen: return 'q';
    case Piece::B_King: return 'k';
    default: return '.';
  }
}

Piece char_to_piece(char c) {
  switch (c) {
    case 'P': return Piece::W_Pawn;
    case 'N': return Piece::W_Knight;
    case 'B': return Piece::W_Bishop;
    case 'R': return Piece::W_Rook;
    case 'Q': return Piece::W_Queen;
    case 'K': return Piece::W_King;
    case 'p': return Piece::B_Pawn;
    case 'n': return Piece::B_Knight;
    case 'b': return Piece::B_Bishop;
    case 'r': return Piece::B_Rook;
    case 'q': return Piece::B_Queen;
    case 'k': return Piece::B_King;
    default: return Piece::None;
  }
}

void set_piece_bit(std::array<std::array<Bitboard, kNumPieceTypes>, kNumColors>& pieces,
                   Color c, PieceType pt, Square sq, bool on) {
  const Bitboard mask = bb::square_bb(sq);
  if (on) {
    pieces[static_cast<int>(c)][static_cast<int>(pt)] |= mask;
  } else {
    pieces[static_cast<int>(c)][static_cast<int>(pt)] &= ~mask;
  }
}

}  // namespace

Board::Board() {
  init_attacks();
  set_startpos();
}

void Board::clear() {
  for (auto& color : pieces_) {
    for (auto& pt : color) {
      pt = 0;
    }
  }
  color_bb_ = {0, 0};
  occupied_ = 0;
  side_ = Color::White;
  ep_square_ = Square::None;
  castling_rights_ = 0;
  halfmove_clock_ = 0;
  fullmove_number_ = 1;
  zobrist_key_ = 0;
}

void Board::refresh_zobrist() {
  init_zobrist();
  zobrist_key_ = zobrist_hash(*this);
}

void Board::refresh_derived() {
  color_bb_[0] = 0;
  color_bb_[1] = 0;
  for (int c = 0; c < 2; ++c) {
    for (int pt = 0; pt < kNumPieceTypes; ++pt) {
      color_bb_[c] |= pieces_[c][pt];
    }
  }
  occupied_ = color_bb_[0] | color_bb_[1];
}

void Board::set_startpos() {
  set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

bool Board::set_fen(const std::string& fen) {
  clear();
  std::istringstream ss(fen);
  std::string board_part;
  ss >> board_part;
  if (board_part.empty()) {
    return false;
  }

  int rank = 7;
  int file = 0;
  for (char ch : board_part) {
    if (ch == '/') {
      if (file != 8) {
        return false;
      }
      rank--;
      file = 0;
      continue;
    }
    if (ch >= '1' && ch <= '8') {
      const int skip = ch - '0';
      if (file + skip > 8) {
        return false;
      }
      file += skip;
      continue;
    }
    if (file >= 8) {
      return false;
    }
    const Piece p = char_to_piece(ch);
    if (p == Piece::None) {
      return false;
    }
    const Square sq = bb::make_square(file, rank);
    const Color c = piece_color(p);
    const PieceType pt = piece_type(p);
    set_piece_bit(pieces_, c, pt, sq, true);
    file++;
  }
  if (file != 8 || rank != 0) {
    return false;
  }

  char stm = 0;
  ss >> stm;
  side_ = (stm == 'b') ? Color::Black : Color::White;

  std::string castling;
  ss >> castling;
  castling_rights_ = 0;
  for (char c : castling) {
    if (c == 'K') castling_rights_ |= kCastleWK;
    if (c == 'Q') castling_rights_ |= kCastleWQ;
    if (c == 'k') castling_rights_ |= kCastleBK;
    if (c == 'q') castling_rights_ |= kCastleBQ;
  }

  std::string ep;
  ss >> ep;
  if (ep != "-" && ep.size() >= 2) {
    const int f = ep[0] - 'a';
    const int r = ep[1] - '1';
    ep_square_ = bb::make_square(f, r);
  } else {
    ep_square_ = Square::None;
  }

  ss >> halfmove_clock_ >> fullmove_number_;
  refresh_derived();
  refresh_zobrist();
  return true;
}

Piece Board::piece_on(Square sq) const {
  const Bitboard mask = bb::square_bb(sq);
  for (int c = 0; c < 2; ++c) {
    for (int pt = 0; pt < kNumPieceTypes; ++pt) {
      if (pieces_[c][pt] & mask) {
        return make_piece(static_cast<Color>(c), static_cast<PieceType>(pt));
      }
    }
  }
  return Piece::None;
}

bool Board::in_check(Color c) const {
  const Bitboard king_bb = pieces_[static_cast<int>(c)][static_cast<int>(PieceType::King)];
  if (!king_bb) {
    return false;
  }
  const Square king_sq = static_cast<Square>(bb::lsb_index(king_bb));
  return attackers_to(*this, king_sq, !c) != 0;
}

void Board::update_occupied(Color c, PieceType pt, Square sq, bool add) {
  const Bitboard mask = bb::square_bb(sq);
  if (add) {
    pieces_[static_cast<int>(c)][static_cast<int>(pt)] |= mask;
  } else {
    pieces_[static_cast<int>(c)][static_cast<int>(pt)] &= ~mask;
  }
  refresh_derived();
}

void Board::make_move(const Move& move, Undo& undo) {
  undo.move = move;
  undo.ep_square = ep_square_;
  undo.castling_rights = castling_rights_;
  undo.halfmove_clock = halfmove_clock_;
  undo.captured_piece = Piece::None;
  undo.captured_bb = 0;

  const Color us = side_;
  const Color them = !us;
  const Bitboard from_mask = bb::square_bb(move.from);
  const Bitboard to_mask = bb::square_bb(move.to);

  PieceType moving = PieceType::None;
  for (int pt = 0; pt < kNumPieceTypes; ++pt) {
    if (pieces_[static_cast<int>(us)][pt] & from_mask) {
      moving = static_cast<PieceType>(pt);
      break;
    }
  }
  undo.moved_piece = moving;

  ep_square_ = Square::None;

  if (move.flag == MoveFlag::Castle) {
    const bool king_side = bb::file_of(move.to) > bb::file_of(move.from);
    const Square rook_from =
        king_side ? bb::make_square(7, bb::rank_of(move.from)) : bb::make_square(0, bb::rank_of(move.from));
    const Square rook_to =
        king_side ? bb::make_square(5, bb::rank_of(move.from)) : bb::make_square(3, bb::rank_of(move.from));

    set_piece_bit(pieces_, us, PieceType::King, move.from, false);
    set_piece_bit(pieces_, us, PieceType::King, move.to, true);
    set_piece_bit(pieces_, us, PieceType::Rook, rook_from, false);
    set_piece_bit(pieces_, us, PieceType::Rook, rook_to, true);
    halfmove_clock_++;
  } else {
    if (occupied_ & to_mask) {
      for (int pt = 0; pt < kNumPieceTypes; ++pt) {
        if (pieces_[static_cast<int>(them)][pt] & to_mask) {
          undo.captured_piece = make_piece(them, static_cast<PieceType>(pt));
          undo.captured_bb = to_mask;
          set_piece_bit(pieces_, them, static_cast<PieceType>(pt), move.to, false);
          break;
        }
      }
    }

    set_piece_bit(pieces_, us, moving, move.from, false);

    if (move.flag == MoveFlag::EnPassant) {
      const Square cap_sq =
          us == Color::White ? bb::shift_down(move.to) : bb::shift_up(move.to);
      set_piece_bit(pieces_, them, PieceType::Pawn, cap_sq, false);
      undo.captured_piece = make_piece(them, PieceType::Pawn);
      undo.captured_bb = bb::square_bb(cap_sq);
    }

    PieceType place = moving;
    if (move.promotion != PieceType::None) {
      place = move.promotion;
    }
    set_piece_bit(pieces_, us, place, move.to, true);

    if (moving == PieceType::Pawn) {
      if (move.flag == MoveFlag::DoublePush) {
        ep_square_ = us == Color::White ? bb::shift_down(move.to) : bb::shift_up(move.to);
      }
      halfmove_clock_ = 0;
    } else if (move.flag == MoveFlag::Capture || move.flag == MoveFlag::PromotionCapture ||
               move.flag == MoveFlag::EnPassant) {
      halfmove_clock_ = 0;
    } else {
      halfmove_clock_++;
    }
  }

  if (undo.moved_piece == PieceType::King) {
    if (us == Color::White) {
      castling_rights_ &= ~(kCastleWK | kCastleWQ);
    } else {
      castling_rights_ &= ~(kCastleBK | kCastleBQ);
    }
  }
  if (undo.moved_piece == PieceType::Rook) {
    if (move.from == Square::A1) castling_rights_ &= ~kCastleWQ;
    if (move.from == Square::H1) castling_rights_ &= ~kCastleWK;
    if (move.from == Square::A8) castling_rights_ &= ~kCastleBQ;
    if (move.from == Square::H8) castling_rights_ &= ~kCastleBK;
  }
  if (undo.captured_piece == make_piece(Color::White, PieceType::Rook) && move.to == Square::A1) {
    castling_rights_ &= ~kCastleWQ;
  }
  if (undo.captured_piece == make_piece(Color::White, PieceType::Rook) && move.to == Square::H1) {
    castling_rights_ &= ~kCastleWK;
  }
  if (undo.captured_piece == make_piece(Color::Black, PieceType::Rook) && move.to == Square::A8) {
    castling_rights_ &= ~kCastleBQ;
  }
  if (undo.captured_piece == make_piece(Color::Black, PieceType::Rook) && move.to == Square::H8) {
    castling_rights_ &= ~kCastleBK;
  }

  if (side_ == Color::Black) {
    fullmove_number_++;
  }
  side_ = them;
  refresh_derived();
  refresh_zobrist();
}

void Board::unmake_move(const Move& move, const Undo& undo) {
  side_ = !side_;
  const Color us = side_;
  const Color them = !us;

  if (move.flag == MoveFlag::Castle) {
    const bool king_side = bb::file_of(move.to) > bb::file_of(move.from);
    const Square rook_from =
        king_side ? bb::make_square(7, bb::rank_of(move.from)) : bb::make_square(0, bb::rank_of(move.from));
    const Square rook_to =
        king_side ? bb::make_square(5, bb::rank_of(move.from)) : bb::make_square(3, bb::rank_of(move.from));

    set_piece_bit(pieces_, us, PieceType::King, move.to, false);
    set_piece_bit(pieces_, us, PieceType::King, move.from, true);
    set_piece_bit(pieces_, us, PieceType::Rook, rook_to, false);
    set_piece_bit(pieces_, us, PieceType::Rook, rook_from, true);
  } else {
    const PieceType moving = undo.moved_piece;
    if (move.promotion != PieceType::None) {
      set_piece_bit(pieces_, us, move.promotion, move.to, false);
    } else {
      set_piece_bit(pieces_, us, moving, move.to, false);
    }

    set_piece_bit(pieces_, us, moving, move.from, true);

    if (move.flag == MoveFlag::EnPassant) {
      const Square cap_sq =
          us == Color::White ? bb::shift_down(move.to) : bb::shift_up(move.to);
      set_piece_bit(pieces_, them, PieceType::Pawn, cap_sq, true);
    } else if (undo.captured_piece != Piece::None) {
      set_piece_bit(pieces_, them, piece_type(undo.captured_piece), move.to, true);
    }
  }

  ep_square_ = undo.ep_square;
  castling_rights_ = undo.castling_rights;
  halfmove_clock_ = undo.halfmove_clock;
  if (side_ == Color::Black) {
    fullmove_number_--;
  }
  refresh_derived();
  refresh_zobrist();
}

bool Board::is_checkmate() const {
  if (!in_check(side_)) {
    return false;
  }
  MoveList list;
  generate_legal_moves(*this, list);
  return list.count == 0;
}

bool Board::is_stalemate() const {
  if (in_check(side_)) {
    return false;
  }
  MoveList list;
  generate_legal_moves(*this, list);
  return list.count == 0;
}

std::string Board::fen() const {
  std::ostringstream out;
  for (int rank = 7; rank >= 0; --rank) {
    int empty = 0;
    for (int file = 0; file < 8; ++file) {
      const Square sq = bb::make_square(file, rank);
      const Piece p = piece_on(sq);
      if (p == Piece::None) {
        empty++;
      } else {
        if (empty) {
          out << empty;
          empty = 0;
        }
        out << piece_to_char(p);
      }
    }
    if (empty) {
      out << empty;
    }
    if (rank > 0) {
      out << '/';
    }
  }
  out << (side_ == Color::White ? " w " : " b ");
  if (castling_rights_ == 0) {
    out << '-';
  } else {
    if (castling_rights_ & kCastleWK) out << 'K';
    if (castling_rights_ & kCastleWQ) out << 'Q';
    if (castling_rights_ & kCastleBK) out << 'k';
    if (castling_rights_ & kCastleBQ) out << 'q';
  }
  out << ' ';
  if (ep_square_ == Square::None) {
    out << '-';
  } else {
    out << char('a' + bb::file_of(ep_square_));
    out << char('1' + bb::rank_of(ep_square_));
  }
  out << ' ' << halfmove_clock_ << ' ' << fullmove_number_;
  return out.str();
}

}  // namespace engine
