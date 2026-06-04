#include "search_eval_layered.hpp"

#include "engine/attacks.hpp"
#include "engine/movegen.hpp"
#include "search_eval_phase.hpp"

namespace engine::search_common {

namespace {

constexpr Bitboard file_bb(int file) { return bb::kFileA << file; }

constexpr Bitboard rank_bb(int rank) { return bb::kRank1 << (rank * 8); }

static bool friendly_pawn_on_adjacent_file(const Board& board, Color c, int file) {
  const Bitboard pawns = board.pieces(c, PieceType::Pawn);
  if (file > 0 && (pawns & file_bb(file - 1))) {
    return true;
  }
  if (file < 7 && (pawns & file_bb(file + 1))) {
    return true;
  }
  return false;
}

static void pawn_structure_score_split(const Board& board, Color c, int& mg, int& eg) {
  mg = 0;
  eg = 0;

  const Bitboard pawns = board.pieces(c, PieceType::Pawn);

  int files_count[8] = {};
  Bitboard scan = pawns;
  while (scan) {
    const Square sq = bb::pop_lsb(scan);
    ++files_count[bb::file_of(sq)];
  }

  for (int file = 0; file < 8; ++file) {
    if (files_count[file] > 1) {
      mg -= 15 * (files_count[file] - 1);
    }
  }

  scan = pawns;
  while (scan) {
    const Square sq = bb::pop_lsb(scan);
    const int file = bb::file_of(sq);
    const int rank = bb::rank_of(sq);

    if (!friendly_pawn_on_adjacent_file(board, c, file)) {
      mg -= 18;
    }

    // Passed pawns: scored only in evaluate_passed_pawns_split (v21+). No EG bonus here
    // to avoid double-counting when layered pawn is combined with that module.

    const int home_rank = c == Color::White ? 1 : 6;
    if (rank == home_rank && files_count[file] > 1) {
      mg -= 8;
    }
  }
}

static int unique_king_zone_attackers(const Board& board, Square king_sq, Color by_color) {
  Bitboard zone_attackers = 0;
  Bitboard zone = king_attacks(king_sq);
  while (zone) {
    const Square sq = bb::pop_lsb(zone);
    zone_attackers |= attackers_to(board, sq, by_color);
  }
  return bb::popcount(zone_attackers);
}

static void king_safety_score_split(const Board& board, Color c, int& mg, int& eg) {
  mg = 0;
  eg = 0;

  const Bitboard kings = board.pieces(c, PieceType::King);
  if (!kings) {
    return;
  }

  const Square king_sq = static_cast<Square>(bb::lsb_index(kings));
  const int file = bb::file_of(king_sq);
  const int rank = bb::rank_of(king_sq);
  const Color them = !c;

  mg -= unique_king_zone_attackers(board, king_sq, them) * 12;

  const Bitboard my_pawns = board.pieces(c, PieceType::Pawn);
  for (int df = -1; df <= 1; ++df) {
    const int f = file + df;
    if (f < 0 || f > 7) {
      continue;
    }
    if ((my_pawns & file_bb(f)) == 0) {
      mg -= 12;
      eg -= 4;
      if ((board.pieces(them, PieceType::Pawn) & file_bb(f)) != 0) {
        mg -= 8;
      }
    }
  }

  Bitboard shield_mask = 0;
  if (c == Color::White) {
    if (rank < 7) {
      shield_mask = rank_bb(rank + 1);
      if (rank < 6) {
        shield_mask |= rank_bb(rank + 2);
      }
    }
  } else if (rank > 0) {
    shield_mask = rank_bb(rank - 1);
    if (rank > 1) {
      shield_mask |= rank_bb(rank - 2);
    }
  }

  if (shield_mask) {
    Bitboard shield_zone = 0;
    for (int df = -1; df <= 1; ++df) {
      const int f = file + df;
      if (f >= 0 && f <= 7) {
        shield_zone |= file_bb(f);
      }
    }
    shield_zone &= shield_mask;
    mg += bb::popcount(my_pawns & shield_zone) * 9;
  }

  if (game_phase(board) >= kPhaseMax / 2) {
    if (file >= 2 && file <= 5 && rank >= 2 && rank <= 5) {
      mg -= 25;
    }
    if (rank >= (c == Color::White ? 4 : 0) && rank <= (c == Color::White ? 7 : 3)) {
      mg -= 10;
    }
  }
}

}  // namespace

void evaluate_pawn_structure_split(const Board& board, int& mg_white, int& eg_white,
                                   int& mg_black, int& eg_black) {
  pawn_structure_score_split(board, Color::White, mg_white, eg_white);
  pawn_structure_score_split(board, Color::Black, mg_black, eg_black);
}

void evaluate_king_safety_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                                int& eg_black) {
  king_safety_score_split(board, Color::White, mg_white, eg_white);
  king_safety_score_split(board, Color::Black, mg_black, eg_black);
}

void evaluate_layered_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                            int& eg_black) {
  evaluate_pawn_structure_split(board, mg_white, eg_white, mg_black, eg_black);

  int king_mg_w = 0;
  int king_eg_w = 0;
  int king_mg_b = 0;
  int king_eg_b = 0;
  evaluate_king_safety_split(board, king_mg_w, king_eg_w, king_mg_b, king_eg_b);

  mg_white += king_mg_w;
  eg_white += king_eg_w;
  mg_black += king_mg_b;
  eg_black += king_eg_b;
}

}  // namespace engine::search_common
