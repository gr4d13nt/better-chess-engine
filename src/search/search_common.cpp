#include "search_common.hpp"

namespace engine::search_common {

namespace {

constexpr int kPieceValues[kNumPieceTypes] = {
    100,  // Pawn
    320,  // Knight
    330,  // Bishop
    500,  // Rook
    900,  // Queen
    0,    // King
};

constexpr int kMobilityWeight[kNumPieceTypes] = {0, 4, 4, 2, 1, 0};

constexpr int kPawnPst[64] = {
     0,   0,   0,   0,   0,   0,   0,   0,
    50,  50,  50,  50,  50,  50,  50,  50,
    10,  10,  20,  30,  30,  20,  10,  10,
     5,   5,  10,  25,  25,  10,   5,   5,
     0,   0,   0,  20,  20,   0,   0,   0,
     5,  -5, -10,   0,   0, -10,  -5,   5,
     5,  10,  10, -20, -20,  10,  10,   5,
     0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr int kKnightPst[64] = {
   -50, -40, -30, -30, -30, -30, -40, -50,
   -40, -20,   0,   5,   5,   0, -20, -40,
   -30,   5,  10,  15,  15,  10,   5, -30,
   -30,   0,  15,  20,  20,  15,   0, -30,
   -30,   5,  15,  20,  20,  15,   5, -30,
   -30,   0,  10,  15,  15,  10,   0, -30,
   -40, -20,   0,   0,   0,   0, -20, -40,
   -50, -40, -30, -30, -30, -30, -40, -50,
};

constexpr int kBishopPst[64] = {
   -20, -10, -10, -10, -10, -10, -10, -20,
   -10,   5,   0,   0,   0,   0,   5, -10,
   -10,  10,  10,  10,  10,  10,  10, -10,
   -10,   0,  10,  10,  10,  10,   0, -10,
   -10,   5,   5,  10,  10,   5,   5, -10,
   -10,   0,   5,  10,  10,   5,   0, -10,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -20, -10, -10, -10, -10, -10, -10, -20,
};

constexpr int kRookPst[64] = {
     0,   0,   0,   5,   5,   0,   0,   0,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
     5,  10,  10,  10,  10,  10,  10,   5,
     0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr int kQueenPst[64] = {
   -20, -10, -10,  -5,  -5, -10, -10, -20,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
    -5,   0,   5,   5,   5,   5,   0,  -5,
     0,   0,   5,   5,   5,   5,   0,  -5,
   -10,   5,   5,   5,   5,   5,   0, -10,
   -10,   0,   5,   0,   0,   0,   0, -10,
   -20, -10, -10,  -5,  -5, -10, -10, -20,
};

constexpr int kKingPst[64] = {
    20,  30,  10,   0,   0,  10,  30,  20,
    20,  20,   0,   0,   0,   0,  20,  20,
   -10, -20, -20, -20, -20, -20, -20, -10,
   -20, -30, -30, -40, -40, -30, -30, -20,
   -30, -40, -40, -50, -50, -40, -40, -30,
   -30, -40, -40, -50, -50, -40, -40, -30,
   -30, -40, -40, -50, -50, -40, -40, -30,
   -30, -40, -40, -50, -50, -40, -40, -30,
};

int mirror_square(int sq) {
  const int file = sq & 7;
  const int rank = sq >> 3;
  return (7 - rank) * 8 + file;
}

int pst_value(PieceType pt, int sq) {
  switch (pt) {
    case PieceType::Pawn: return kPawnPst[sq];
    case PieceType::Knight: return kKnightPst[sq];
    case PieceType::Bishop: return kBishopPst[sq];
    case PieceType::Rook: return kRookPst[sq];
    case PieceType::Queen: return kQueenPst[sq];
    case PieceType::King: return kKingPst[sq];
    default: return 0;
  }
}

int mobility_score(const Board& board, Color c) {
  const Bitboard occ = board.occupied();
  const Bitboard own = board.color_bb(c);
  int score = 0;

  auto add_mobility = [&](PieceType pt, auto attack_fn) {
    Bitboard pieces = board.pieces(c, pt);
    while (pieces) {
      const Square sq = bb::pop_lsb(pieces);
      const Bitboard attacks = attack_fn(sq) & ~own;
      score += bb::popcount(attacks) * kMobilityWeight[static_cast<int>(pt)];
    }
  };

  add_mobility(PieceType::Knight, [&](Square sq) { return knight_attacks(sq); });
  add_mobility(PieceType::Bishop, [&](Square sq) { return bishop_attacks(sq, occ); });
  add_mobility(PieceType::Rook, [&](Square sq) { return rook_attacks(sq, occ); });
  add_mobility(PieceType::Queen, [&](Square sq) { return queen_attacks(sq, occ); });

  return score;
}

}  // namespace

bool should_stop(SearchState& st) {
  if (!st.use_time) {
    return false;
  }
  // Poll clock periodically to reduce overhead.
  if ((st.nodes & 2047) != 0) {
    return false;
  }
  if (std::chrono::steady_clock::now() >= st.deadline) {
    st.stopped = true;
    return true;
  }
  return false;
}

int evaluate_material_only(const Board& board) {
  int white = 0;
  int black = 0;
  for (int pt = 0; pt < kNumPieceTypes; ++pt) {
    const int value = kPieceValues[pt];
    white += bb::popcount(board.pieces(Color::White, static_cast<PieceType>(pt))) * value;
    black += bb::popcount(board.pieces(Color::Black, static_cast<PieceType>(pt))) * value;
  }
  return white - black;
}

int evaluate_improved(const Board& board) {
  int white = 0;
  int black = 0;

  for (int pt = 0; pt < kNumPieceTypes; ++pt) {
    const PieceType piece_type = static_cast<PieceType>(pt);
    const int base = kPieceValues[pt];
    Bitboard w = board.pieces(Color::White, piece_type);
    Bitboard b = board.pieces(Color::Black, piece_type);
    while (w) {
      const int sq = square_index(bb::pop_lsb(w));
      white += base + pst_value(piece_type, sq);
    }
    while (b) {
      const int sq = square_index(bb::pop_lsb(b));
      black += base + pst_value(piece_type, mirror_square(sq));
    }
  }

  if (bb::popcount(board.pieces(Color::White, PieceType::Bishop)) >= 2) {
    white += 30;
  }
  if (bb::popcount(board.pieces(Color::Black, PieceType::Bishop)) >= 2) {
    black += 30;
  }

  white += mobility_score(board, Color::White);
  black += mobility_score(board, Color::Black);
  return white - black;
}

int evaluate_for_side_to_move(const Board& board, EngineVersion eval_profile) {
  const int white_eval =
      eval_profile == EngineVersion::V3_ImprovedEval ? evaluate_improved(board)
                                                     : evaluate_material_only(board);
  return board.side_to_move() == Color::White ? white_eval : -white_eval;
}

}  // namespace engine::search_common
