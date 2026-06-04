#include "search_common.hpp"

#include "search_eval_bishop.hpp"
#include "search_eval_knight.hpp"
#include "search_eval_layered.hpp"
#include "search_eval_mopup.hpp"
#include "search_eval_phase.hpp"
#include "search_eval_rook.hpp"

namespace engine::search_common {
namespace {

constexpr int kTempoBonus = 18;

constexpr int kPassedMgByAdvance[8] = {0, 6, 12, 20, 32, 50, 75, 0};
constexpr int kPassedEgByAdvance[8] = {0, 10, 20, 36, 60, 95, 140, 0};
constexpr int kConnectedPassedMg = 12;
constexpr int kConnectedPassedEg = 22;
constexpr int kProtectedPassedMg = 10;
constexpr int kProtectedPassedEg = 20;
constexpr int kBlockadedPassedMg = -12;
constexpr int kBlockadedPassedEg = -28;

constexpr Bitboard file_bb(int file) { return bb::kFileA << file; }
constexpr Bitboard rank_bb(int rank) { return bb::kRank1 << (rank * 8); }

Bitboard ranks_from(int rank, Color c) {
  if (rank < 0 || rank > 7) {
    return 0;
  }
  Bitboard mask = 0;
  if (c == Color::White) {
    for (int r = rank; r < 8; ++r) {
      mask |= rank_bb(r);
    }
  } else {
    for (int r = rank; r >= 0; --r) {
      mask |= rank_bb(r);
    }
  }
  return mask;
}

bool is_passed_pawn(const Board& board, Color c, Square sq) {
  const int file = bb::file_of(sq);
  const int rank = bb::rank_of(sq);
  const Color them = !c;

  Bitboard zone = file_bb(file);
  if (file > 0) {
    zone |= file_bb(file - 1);
  }
  if (file < 7) {
    zone |= file_bb(file + 1);
  }

  const Bitboard enemy_pawns = board.pieces(them, PieceType::Pawn) & zone;
  if (c == Color::White) {
    return (enemy_pawns & ranks_from(rank + 1, Color::White)) == 0;
  }
  return (enemy_pawns & ranks_from(rank - 1, Color::Black)) == 0;
}

bool has_adjacent_friendly_pawn(const Board& board, Color c, Square sq) {
  const int file = bb::file_of(sq);
  const int rank = bb::rank_of(sq);
  const Bitboard pawns = board.pieces(c, PieceType::Pawn);
  for (int df = -1; df <= 1; df += 2) {
    const int f = file + df;
    if (f < 0 || f > 7) {
      continue;
    }
    const Bitboard adjacent = pawns & file_bb(f);
    if (adjacent == 0) {
      continue;
    }
    for (int dr = -1; dr <= 1; ++dr) {
      const int r = rank + dr;
      if (r < 0 || r > 7) {
        continue;
      }
      if (adjacent & bb::square_bb(static_cast<Square>(r * 8 + f))) {
        return true;
      }
    }
  }
  return false;
}

bool is_protected_passed(const Board& board, Color c, Square sq) {
  const int file = bb::file_of(sq);
  const int rank = bb::rank_of(sq);
  const Bitboard pawns = board.pieces(c, PieceType::Pawn);
  const int back_rank = c == Color::White ? rank - 1 : rank + 1;
  if (back_rank < 0 || back_rank > 7) {
    return false;
  }
  if (file > 0 &&
      (pawns & bb::square_bb(static_cast<Square>(back_rank * 8 + (file - 1))))) {
    return true;
  }
  if (file < 7 &&
      (pawns & bb::square_bb(static_cast<Square>(back_rank * 8 + (file + 1))))) {
    return true;
  }
  return false;
}

bool is_blockaded(const Board& board, Color c, Square sq) {
  const int file = bb::file_of(sq);
  const int rank = bb::rank_of(sq);
  const int front_rank = c == Color::White ? rank + 1 : rank - 1;
  if (front_rank < 0 || front_rank > 7) {
    return false;
  }
  const Square front = static_cast<Square>(front_rank * 8 + file);
  const Piece p = board.piece_on(front);
  return p != Piece::None && piece_color(p) != c;
}

}  // namespace

void evaluate_passed_pawns_split(const Board& board, int& mg_white, int& eg_white,
                                 int& mg_black, int& eg_black) {
  mg_white = eg_white = mg_black = eg_black = 0;

  auto eval_side = [&](Color c, int& mg, int& eg) {
    Bitboard pawns = board.pieces(c, PieceType::Pawn);
    while (pawns) {
      const Square sq = bb::pop_lsb(pawns);
      if (!is_passed_pawn(board, c, sq)) {
        continue;
      }
      const int rank = bb::rank_of(sq);
      const int advance = c == Color::White ? rank : (7 - rank);
      mg += kPassedMgByAdvance[advance];
      eg += kPassedEgByAdvance[advance];
      if (has_adjacent_friendly_pawn(board, c, sq)) {
        mg += kConnectedPassedMg;
        eg += kConnectedPassedEg;
      }
      if (is_protected_passed(board, c, sq)) {
        mg += kProtectedPassedMg;
        eg += kProtectedPassedEg;
      }
      if (is_blockaded(board, c, sq)) {
        mg += kBlockadedPassedMg;
        eg += kBlockadedPassedEg;
      }
    }
  };

  eval_side(Color::White, mg_white, eg_white);
  eval_side(Color::Black, mg_black, eg_black);
}

namespace {

int evaluate_v21_piece(const Board& board) {
  int imp_mg_w = 0;
  int imp_eg_w = 0;
  int imp_mg_b = 0;
  int imp_eg_b = 0;
  evaluate_improved_split(board, imp_mg_w, imp_eg_w, imp_mg_b, imp_eg_b);

  int lay_mg_w = 0;
  int lay_eg_w = 0;
  int lay_mg_b = 0;
  int lay_eg_b = 0;
  evaluate_layered_split(board, lay_mg_w, lay_eg_w, lay_mg_b, lay_eg_b);

  int knight_mg_w = 0;
  int knight_eg_w = 0;
  int knight_mg_b = 0;
  int knight_eg_b = 0;
  evaluate_knight_outposts_split(board, knight_mg_w, knight_eg_w, knight_mg_b, knight_eg_b);

  int rook_mg_w = 0;
  int rook_eg_w = 0;
  int rook_mg_b = 0;
  int rook_eg_b = 0;
  evaluate_rook_placement_split(board, rook_mg_w, rook_eg_w, rook_mg_b, rook_eg_b);

  int bishop_mg_w = 0;
  int bishop_eg_w = 0;
  int bishop_mg_b = 0;
  int bishop_eg_b = 0;
  evaluate_bishop_freedom_split(board, bishop_mg_w, bishop_eg_w, bishop_mg_b, bishop_eg_b);

  int pass_mg_w = 0;
  int pass_eg_w = 0;
  int pass_mg_b = 0;
  int pass_eg_b = 0;
  evaluate_passed_pawns_split(board, pass_mg_w, pass_eg_w, pass_mg_b, pass_eg_b);

  const int phase = game_phase(board);
  const int mg_net =
      (imp_mg_w + lay_mg_w + knight_mg_w + rook_mg_w + bishop_mg_w + pass_mg_w) -
      (imp_mg_b + lay_mg_b + knight_mg_b + rook_mg_b + bishop_mg_b + pass_mg_b);
  const int eg_net =
      (imp_eg_w + lay_eg_w + knight_eg_w + rook_eg_w + bishop_eg_w + pass_eg_w) -
      (imp_eg_b + lay_eg_b + knight_eg_b + rook_eg_b + bishop_eg_b + pass_eg_b);

  const int tempo = board.side_to_move() == Color::White ? kTempoBonus : -kTempoBonus;
  return taper_eval(mg_net, eg_net, phase) + tempo;
}

}  // namespace

int evaluate_v21(const Board& board) {
  return evaluate_v21_piece(board) + evaluate_mop_up_net(board);
}

}  // namespace engine::search_common
