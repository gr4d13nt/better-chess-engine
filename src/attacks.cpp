#include "engine/attacks.hpp"

#include <array>
#include <vector>

namespace engine {
namespace {

constexpr int kDirsBishop[] = {9, 7, -7, -9};
constexpr int kDirsRook[] = {8, -8, 1, -1};

#include "magics.inc"

std::array<Bitboard, 64> knight_tbl{};
std::array<Bitboard, 64> king_tbl{};
std::array<std::array<Bitboard, 64>, 2> pawn_tbl{};

struct Magic {
  Bitboard mask = 0;
  Bitboard magic = 0;
  Bitboard* attacks = nullptr;
  int shift = 0;
};

std::array<Magic, 64> rook_magics{};
std::array<Magic, 64> bishop_magics{};
std::vector<Bitboard> attack_pool;

bool on_board(int sq) { return sq >= 0 && sq < 64; }

bool valid_step(int prev, int to, int delta) {
  if (!on_board(to)) {
    return false;
  }
  const int prev_file = prev & 7;
  const int prev_rank = prev >> 3;
  const int to_file = to & 7;
  const int to_rank = to >> 3;
  if (delta == 8) {
    return to_rank == prev_rank + 1;
  }
  if (delta == -8) {
    return to_rank == prev_rank - 1;
  }
  if (delta == 1) {
    return to_rank == prev_rank && to_file == prev_file + 1;
  }
  if (delta == -1) {
    return to_rank == prev_rank && to_file == prev_file - 1;
  }
  if (delta == 9) {
    return to_rank == prev_rank + 1 && to_file == prev_file + 1;
  }
  if (delta == -9) {
    return to_rank == prev_rank - 1 && to_file == prev_file - 1;
  }
  if (delta == 7) {
    return to_rank == prev_rank + 1 && to_file == prev_file - 1;
  }
  if (delta == -7) {
    return to_rank == prev_rank - 1 && to_file == prev_file + 1;
  }
  return false;
}

Bitboard ray_attack(Square sq, Bitboard occ, const int* dirs, int n) {
  Bitboard result = 0;
  const int from = static_cast<int>(sq);
  for (int d = 0; d < n; ++d) {
    int prev = from;
    int s = from + dirs[d];
    while (on_board(s) && valid_step(prev, s, dirs[d])) {
      result |= bb::square_bb(static_cast<Square>(s));
      if (occ & bb::square_bb(static_cast<Square>(s))) {
        break;
      }
      prev = s;
      s += dirs[d];
    }
  }
  return result;
}

unsigned magic_index(Bitboard occupied, Bitboard mask, Bitboard magic, int shift) {
  return static_cast<unsigned>(((occupied & mask) * magic) >> shift);
}

Bitboard sliding_attack(int sq, Bitboard occ, const int* dirs, int n) {
  return ray_attack(static_cast<Square>(sq), occ, dirs, n);
}

void init_slider_magics(const int* shifts, const Bitboard* masks, const Bitboard* magics, Magic* entries,
                        const int* dirs) {
  std::size_t total_entries = 0;
  for (int sq = 0; sq < 64; ++sq) {
    total_entries += static_cast<std::size_t>(1) << bb::popcount(masks[sq]);
  }

  const std::size_t base = attack_pool.size();
  attack_pool.resize(base + total_entries, 0);

  std::size_t offset = base;
  for (int sq = 0; sq < 64; ++sq) {
    const Bitboard mask = masks[sq];
    const int bits = bb::popcount(mask);
    const std::size_t table_size = static_cast<std::size_t>(1) << bits;

    Magic& entry = entries[static_cast<std::size_t>(sq)];
    entry.mask = mask;
    entry.magic = magics[sq];
    entry.shift = shifts[sq];
    entry.attacks = attack_pool.data() + offset;

    Bitboard subset = 0;
    do {
      const unsigned idx = magic_index(subset, mask, magics[sq], shifts[sq]);
      entry.attacks[idx] = sliding_attack(sq, subset, dirs, 4);
      subset = (subset - mask) & mask;
    } while (subset);

    offset += table_size;
  }
}

void init_leapers() {
  for (int sq = 0; sq < 64; ++sq) {
    Bitboard kn = 0;
    for (int d : {17, 15, 10, 6, -6, -10, -15, -17}) {
      const int t = sq + d;
      if (on_board(t) && std::abs((sq & 7) - (t & 7)) <= 2) {
        kn |= bb::square_bb(static_cast<Square>(t));
      }
    }
    knight_tbl[sq] = kn;

    Bitboard kg = 0;
    for (int d : {1, -1, 8, -8, 9, 7, -7, -9}) {
      const int t = sq + d;
      if (on_board(t) && std::abs((sq & 7) - (t & 7)) <= 1) {
        kg |= bb::square_bb(static_cast<Square>(t));
      }
    }
    king_tbl[sq] = kg;
  }

  for (int sq = 0; sq < 64; ++sq) {
    Bitboard w = 0;
    Bitboard b = 0;
    if ((sq >> 3) < 7) {
      if ((sq & 7) > 0) {
        w |= 1ULL << (sq + 7);
      }
      if ((sq & 7) < 7) {
        w |= 1ULL << (sq + 9);
      }
    }
    if ((sq >> 3) > 0) {
      if ((sq & 7) > 0) {
        b |= 1ULL << (sq - 9);
      }
      if ((sq & 7) < 7) {
        b |= 1ULL << (sq - 7);
      }
    }
    pawn_tbl[static_cast<int>(Color::White)][sq] = w;
    pawn_tbl[static_cast<int>(Color::Black)][sq] = b;
  }
}

}  // namespace

void init_attacks() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  init_leapers();
  attack_pool.clear();
  attack_pool.reserve(200000);
  init_slider_magics(kRookShifts, kRookMasks, kRookMagics, rook_magics.data(), kDirsRook);
  init_slider_magics(kBishopShifts, kBishopMasks, kBishopMagics, bishop_magics.data(), kDirsBishop);
}

Bitboard knight_attacks(Square sq) { return knight_tbl[square_index(sq)]; }
Bitboard king_attacks(Square sq) { return king_tbl[square_index(sq)]; }
Bitboard pawn_attacks(Color c, Square sq) {
  return pawn_tbl[static_cast<int>(c)][square_index(sq)];
}

Bitboard bishop_attacks(Square sq, Bitboard occ) {
  const Magic& entry = bishop_magics[square_index(sq)];
  const unsigned idx = magic_index(occ, entry.mask, entry.magic, entry.shift);
  return entry.attacks[idx];
}

Bitboard rook_attacks(Square sq, Bitboard occ) {
  const Magic& entry = rook_magics[square_index(sq)];
  const unsigned idx = magic_index(occ, entry.mask, entry.magic, entry.shift);
  return entry.attacks[idx];
}

Bitboard queen_attacks(Square sq, Bitboard occ) {
  return bishop_attacks(sq, occ) | rook_attacks(sq, occ);
}

}  // namespace engine
