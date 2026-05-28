#pragma once

#include "engine/types.hpp"

#include <cstdint>

namespace engine {

using Bitboard = std::uint64_t;

namespace bb {

constexpr Bitboard kFileA = 0x0101010101010101ULL;
constexpr Bitboard kFileH = 0x8080808080808080ULL;
constexpr Bitboard kRank1 = 0x00000000000000FFULL;
constexpr Bitboard kRank2 = 0x000000000000FF00ULL;
constexpr Bitboard kRank7 = 0x00FF000000000000ULL;
constexpr Bitboard kRank8 = 0xFF00000000000000ULL;

constexpr Bitboard kNotFileA = ~kFileA;
constexpr Bitboard kNotFileH = ~kFileH;

constexpr Bitboard square_bb(Square sq) {
  return 1ULL << static_cast<int>(sq);
}

constexpr Square make_square(int file, int rank) {
  return static_cast<Square>(rank * 8 + file);
}

constexpr int file_of(Square sq) { return static_cast<int>(sq) & 7; }
constexpr int rank_of(Square sq) { return static_cast<int>(sq) >> 3; }

constexpr Square shift_up(Square sq) {
  return static_cast<Square>(static_cast<int>(sq) + 8);
}
constexpr Square shift_down(Square sq) {
  return static_cast<Square>(static_cast<int>(sq) - 8);
}

inline int popcount(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_popcountll(b);
#else
  int count = 0;
  while (b) {
    ++count;
    b &= b - 1;
  }
  return count;
#endif
}

inline int lsb_index(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_ctzll(b);
#else
  int idx = 0;
  while ((b & 1) == 0) {
    b >>= 1;
    ++idx;
  }
  return idx;
#endif
}

inline Square pop_lsb(Bitboard& b) {
  const Square sq = static_cast<Square>(lsb_index(b));
  b &= b - 1;
  return sq;
}

inline Bitboard north(Bitboard b) { return b << 8; }
inline Bitboard south(Bitboard b) { return b >> 8; }
inline Bitboard east(Bitboard b) { return (b & kNotFileH) << 1; }
inline Bitboard west(Bitboard b) { return (b & kNotFileA) >> 1; }

inline Bitboard north_east(Bitboard b) { return (b & kNotFileH) << 9; }
inline Bitboard north_west(Bitboard b) { return (b & kNotFileA) << 7; }
inline Bitboard south_east(Bitboard b) { return (b & kNotFileH) >> 7; }
inline Bitboard south_west(Bitboard b) { return (b & kNotFileA) >> 9; }

}  // namespace bb

}  // namespace engine
