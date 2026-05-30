#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/movegen.hpp"
#include "engine/see.hpp"

#include <iostream>

namespace engine {
namespace {

bool expect_true(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << '\n';
    return false;
  }
  return true;
}

bool expect_gt(int a, int b, const char* msg) {
  if (!(a > b)) {
    std::cerr << "FAIL: " << msg << " (got " << a << " vs " << b << ")\n";
    return false;
  }
  return true;
}

bool expect_lt(int a, int b, const char* msg) {
  if (!(a < b)) {
    std::cerr << "FAIL: " << msg << " (got " << a << " vs " << b << ")\n";
    return false;
  }
  return true;
}

bool expect_eq(int a, int b, const char* msg) {
  if (a != b) {
    std::cerr << "FAIL: " << msg << " (got " << a << " vs " << b << ")\n";
    return false;
  }
  return true;
}

Move find_move(const Board& board, const std::string& uci) {
  MoveList moves;
  generate_legal_moves(board, moves);
  for (int i = 0; i < moves.count; ++i) {
    if (move_to_uci(moves.moves[i]) == uci) {
      return moves.moves[i];
    }
  }
  return Move{};
}

bool test_winning_pawn_capture() {
  Board board;
  if (!board.set_fen("8/8/8/8/4p3/3P4/8/4K2k w - - 0 1")) {
    return expect_true(false, "winning pawn capture FEN parse");
  }
  const Move cap = find_move(board, "d3e4");
  if (cap.from == Square::None) {
    return expect_true(false, "d3e4 move not found");
  }
  return expect_gt(see_capture(board, cap), 0, "undefended pawn capture is winning SEE");
}

bool test_losing_knight_capture() {
  Board board;
  if (!board.set_fen("8/8/4n3/8/3p4/8/4N3/4K2k w - - 0 1")) {
    return expect_true(false, "losing knight capture FEN parse");
  }
  const Move cap = find_move(board, "e2d4");
  if (cap.from == Square::None) {
    return expect_true(false, "e2d4 move not found");
  }
  return expect_lt(see_capture(board, cap), 0, "knight capture into pawn defense loses SEE");
}

bool test_equal_rook_trade() {
  Board board;
  if (!board.set_fen("8/4r3/8/4r3/8/8/4R3/4K2k w - - 0 1")) {
    return expect_true(false, "rook trade FEN parse");
  }
  const Move cap = find_move(board, "e2e5");
  if (cap.from == Square::None) {
    return expect_true(false, "e2e5 move not found");
  }
  return expect_eq(see_capture(board, cap), 0, "equal rook trade has zero SEE");
}

bool test_en_passant_positive() {
  Board board;
  if (!board.set_fen("8/8/4p3/3pP3/8/8/8/4K2k w - d6 0 1")) {
    return expect_true(false, "en passant FEN parse");
  }
  const Move cap = find_move(board, "e5d6");
  if (cap.from == Square::None) {
    return expect_true(false, "e5d6 move not found");
  }
  return expect_gt(see_capture(board, cap), 0, "en passant capture is winning SEE");
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;
  ok = engine::test_winning_pawn_capture() && ok;
  ok = engine::test_losing_knight_capture() && ok;
  ok = engine::test_equal_rook_trade() && ok;
  ok = engine::test_en_passant_positive() && ok;

  if (ok) {
    std::cout << "see_tests: all passed\n";
    return 0;
  }
  std::cerr << "see_tests: failed\n";
  return 1;
}
