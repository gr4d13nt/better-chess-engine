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

constexpr int kKingPstMg[64] = {
    20,  30,  10,   0,   0,  10,  30,  20,
    20,  20,   0,   0,   0,   0,  20,  20,
   -10, -20, -20, -20, -20, -20, -20, -10,
   -20, -30, -30, -40, -40, -30, -30, -20,
   -30, -40, -40, -50, -50, -40, -40, -30,
   -30, -40, -40, -50, -50, -40, -40, -30,
   -30, -40, -40, -50, -50, -40, -40, -30,
   -30, -40, -40, -50, -50, -40, -40, -30,
};

constexpr int kKingPstEg[64] = {
   -50, -30, -30, -30, -30, -30, -30, -50,
   -30, -20, -10, -10, -10, -10, -20, -30,
   -30, -10,  10,  15,  15,  10, -10, -30,
   -30, -10,  15,  25,  25,  15, -10, -30,
   -30, -10,  15,  25,  25,  15, -10, -30,
   -30, -10,  10,  15,  15,  10, -10, -30,
   -30, -20, -10, -10, -10, -10, -20, -30,
   -50, -40, -30, -20, -20, -30, -40, -50,
};

constexpr int kEgMobilityScale = 2;

int mirror_square(int sq) {
  const int file = sq & 7;
  const int rank = sq >> 3;
  return (7 - rank) * 8 + file;
}

int pst_value_mg(PieceType pt, int sq) {
  switch (pt) {
    case PieceType::Pawn: return kPawnPst[sq];
    case PieceType::Knight: return kKnightPst[sq];
    case PieceType::Bishop: return kBishopPst[sq];
    case PieceType::Rook: return kRookPst[sq];
    case PieceType::Queen: return kQueenPst[sq];
    case PieceType::King: return kKingPstMg[sq];
    default: return 0;
  }
}

int pst_value_eg(PieceType pt, int sq, Color c) {
  switch (pt) {
    case PieceType::Pawn: {
      const int rank = sq >> 3;
      const int advancement = c == Color::White ? rank : (7 - rank);
      return kPawnPst[sq] + advancement * 4;
    }
    case PieceType::Knight: return kKnightPst[sq];
    case PieceType::Bishop: return kBishopPst[sq];
    case PieceType::Rook: return kRookPst[sq];
    case PieceType::Queen: return kQueenPst[sq];
    case PieceType::King: return kKingPstEg[sq];
    default: return 0;
  }
}

int mobility_score(const Board& board, Color c, int weight_scale) {
  const Bitboard occ = board.occupied();
  const Bitboard own = board.color_bb(c);
  int score = 0;

  auto add_mobility = [&](PieceType pt, auto attack_fn) {
    Bitboard pieces = board.pieces(c, pt);
    while (pieces) {
      const Square sq = bb::pop_lsb(pieces);
      const Bitboard attacks = attack_fn(sq) & ~own;
      score += bb::popcount(attacks) * kMobilityWeight[static_cast<int>(pt)] * weight_scale;
    }
  };

  add_mobility(PieceType::Knight, [&](Square sq) { return knight_attacks(sq); });
  add_mobility(PieceType::Bishop, [&](Square sq) { return bishop_attacks(sq, occ); });
  add_mobility(PieceType::Rook, [&](Square sq) { return rook_attacks(sq, occ); });
  add_mobility(PieceType::Queen, [&](Square sq) { return queen_attacks(sq, occ); });

  return score;
}

void accumulate_improved_side(const Board& board, Color c, int& mg, int& eg) {
  mg = 0;
  eg = 0;

  for (int pt = 0; pt < kNumPieceTypes; ++pt) {
    const PieceType piece_type = static_cast<PieceType>(pt);
    const int base = kPieceValues[pt];
    Bitboard pieces = board.pieces(c, piece_type);
    while (pieces) {
      const int sq = square_index(bb::pop_lsb(pieces));
      const int mirrored = mirror_square(sq);
      mg += base + pst_value_mg(piece_type, c == Color::White ? sq : mirrored);
      eg += base + pst_value_eg(piece_type, c == Color::White ? sq : mirrored, c);
    }
  }

  if (bb::popcount(board.pieces(c, PieceType::Bishop)) >= 2) {
    mg += 30;
  }

  mg += mobility_score(board, c, 1);
  eg += mobility_score(board, c, kEgMobilityScale);
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
      white += base + pst_value_mg(piece_type, sq);
    }
    while (b) {
      const int sq = square_index(bb::pop_lsb(b));
      black += base + pst_value_mg(piece_type, mirror_square(sq));
    }
  }

  if (bb::popcount(board.pieces(Color::White, PieceType::Bishop)) >= 2) {
    white += 30;
  }
  if (bb::popcount(board.pieces(Color::Black, PieceType::Bishop)) >= 2) {
    black += 30;
  }

  white += mobility_score(board, Color::White, 1);
  black += mobility_score(board, Color::Black, 1);
  return white - black;
}

void evaluate_improved_split(const Board& board, int& mg_white, int& eg_white, int& mg_black,
                             int& eg_black) {
  accumulate_improved_side(board, Color::White, mg_white, eg_white);
  accumulate_improved_side(board, Color::Black, mg_black, eg_black);
}

int evaluate_for_side_to_move(const Board& board, EngineVersion eval_profile) {
  int white_eval = 0;
  if (eval_profile == EngineVersion::V13_MopUpEval) {
    white_eval = evaluate_v13(board);
  } else if (eval_profile == EngineVersion::V9_RookPlacement) {
    white_eval = evaluate_v9(board);
  } else if (eval_profile == EngineVersion::V8_KnightOutposts) {
    white_eval = evaluate_v8(board);
  } else if (eval_profile == EngineVersion::V7_PhasedEval) {
    white_eval = evaluate_v7(board);
  } else if (eval_profile == EngineVersion::V6_PawnKingEval) {
    white_eval = evaluate_v6(board);
  } else if (eval_profile == EngineVersion::V3_ImprovedEval) {
    white_eval = evaluate_improved(board);
  } else {
    white_eval = evaluate_material_only(board);
  }
  return board.side_to_move() == Color::White ? white_eval : -white_eval;
}

}  // namespace engine::search_common
