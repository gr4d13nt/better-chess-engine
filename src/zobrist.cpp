#include "engine/zobrist.hpp"

#include "engine/board.hpp"

#include <array>
#include <random>

namespace engine {
namespace {

std::array<std::array<std::array<std::uint64_t, 64>, kNumPieceTypes>, kNumColors> piece_zobrist{};
std::uint64_t side_zobrist = 0;
std::array<std::uint64_t, 16> castling_zobrist{};
std::array<std::uint64_t, 8> ep_file_zobrist{};
bool initialized = false;

std::uint64_t random_u64(std::mt19937_64& rng) {
  return rng();
}

void init_tables() {
  if (initialized) {
    return;
  }
  std::mt19937_64 rng(0xC0FFEEULL);
  for (int c = 0; c < kNumColors; ++c) {
    for (int pt = 0; pt < kNumPieceTypes; ++pt) {
      for (int sq = 0; sq < 64; ++sq) {
        piece_zobrist[c][pt][sq] = random_u64(rng);
      }
    }
  }
  side_zobrist = random_u64(rng);
  for (int i = 0; i < 16; ++i) {
    castling_zobrist[i] = random_u64(rng);
  }
  for (int f = 0; f < 8; ++f) {
    ep_file_zobrist[f] = random_u64(rng);
  }
  initialized = true;
}

}  // namespace

void init_zobrist() { init_tables(); }

std::uint64_t zobrist_hash(const Board& board) {
  init_tables();

  std::uint64_t hash = 0;
  for (int c = 0; c < kNumColors; ++c) {
    for (int pt = 0; pt < kNumPieceTypes; ++pt) {
      Bitboard bb = board.pieces(static_cast<Color>(c), static_cast<PieceType>(pt));
      while (bb) {
        const Square sq = bb::pop_lsb(bb);
        hash ^= piece_zobrist[c][pt][square_index(sq)];
      }
    }
  }

  if (board.side_to_move() == Color::White) {
    hash ^= side_zobrist;
  }

  hash ^= castling_zobrist[board.castling_rights() & 0xF];

  const Square ep = board.ep_square();
  if (ep != Square::None) {
    hash ^= ep_file_zobrist[bb::file_of(ep)];
  }

  return hash;
}

std::uint64_t zobrist_piece_key(Color color, PieceType piece_type, Square square) {
  init_tables();
  return piece_zobrist[static_cast<int>(color)][static_cast<int>(piece_type)][square_index(square)];
}

std::uint64_t zobrist_side_key() {
  init_tables();
  return side_zobrist;
}

std::uint64_t zobrist_castling_key(std::uint8_t castling_rights) {
  init_tables();
  return castling_zobrist[castling_rights & 0xF];
}

std::uint64_t zobrist_ep_key(Square ep_square) {
  if (ep_square == Square::None) {
    return 0;
  }
  init_tables();
  return ep_file_zobrist[bb::file_of(ep_square)];
}

}  // namespace engine
