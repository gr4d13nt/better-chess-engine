#pragma once

#include "engine/types.hpp"

#include <cstdint>
#include <string>

namespace engine {

enum class MoveFlag : uint8_t {
  Quiet,
  Capture,
  DoublePush,
  EnPassant,
  Castle,
  Promotion,
  PromotionCapture,
};

struct Move {
  Square from = Square::None;
  Square to = Square::None;
  PieceType promotion = PieceType::None;
  MoveFlag flag = MoveFlag::Quiet;
  PieceType captured = PieceType::None;

  constexpr Move() = default;
  constexpr Move(Square from, Square to, MoveFlag flag = MoveFlag::Quiet)
      : from(from), to(to), flag(flag) {}
};

struct MoveList {
  static constexpr int kMaxMoves = 256;
  Move moves[kMaxMoves];
  int count = 0;

  void add(const Move& m) { moves[count++] = m; }
  void clear() { count = 0; }
};

std::string move_to_uci(const Move& m);

}  // namespace engine
