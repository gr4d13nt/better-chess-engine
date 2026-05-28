#pragma once

#include <cstdint>
#include <string>

namespace engine {

enum class Color : uint8_t { White, Black };

constexpr Color operator!(Color c) {
  return c == Color::White ? Color::Black : Color::White;
}

enum class PieceType : uint8_t {
  Pawn,
  Knight,
  Bishop,
  Rook,
  Queen,
  King,
  None,
};

enum class Piece : uint8_t {
  W_Pawn,
  W_Knight,
  W_Bishop,
  W_Rook,
  W_Queen,
  W_King,
  B_Pawn,
  B_Knight,
  B_Bishop,
  B_Rook,
  B_Queen,
  B_King,
  None,
};

constexpr int kNumPieceTypes = 6;
constexpr int kNumColors = 2;

constexpr PieceType piece_type(Piece p) {
  return static_cast<PieceType>(static_cast<uint8_t>(p) % 6);
}

constexpr Color piece_color(Piece p) {
  return static_cast<Color>(static_cast<uint8_t>(p) / 6);
}

constexpr Piece make_piece(Color c, PieceType pt) {
  return static_cast<Piece>(static_cast<uint8_t>(c) * 6 + static_cast<uint8_t>(pt));
}

enum class Square : int {
  A1, B1, C1, D1, E1, F1, G1, H1,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A8, B8, C8, D8, E8, F8, G8, H8,
  None = 64,
};

constexpr int square_index(Square sq) { return static_cast<int>(sq); }

}  // namespace engine
