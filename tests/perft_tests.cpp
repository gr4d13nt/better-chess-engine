#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/movegen.hpp"

#include <cstdint>
#include <iostream>
#include <string>

namespace engine {
namespace {

std::uint64_t perft(Board& board, int depth) {
  if (depth == 0) {
    return 1;
  }

  MoveList moves;
  generate_legal_moves(board, moves);

  if (depth == 1) {
    return static_cast<std::uint64_t>(moves.count);
  }

  std::uint64_t nodes = 0;
  for (int i = 0; i < moves.count; ++i) {
    Undo undo;
    board.make_move(moves.moves[i], undo);
    nodes += perft(board, depth - 1);
    board.unmake_move(moves.moves[i], undo);
  }
  return nodes;
}

bool expect_perft(const std::string& fen, int depth, std::uint64_t expected) {
  Board board;
  if (fen == "startpos") {
    board.set_startpos();
  } else if (!board.set_fen(fen)) {
    std::cerr << "FAIL fen parse: " << fen << '\n';
    return false;
  }
  const std::uint64_t nodes = perft(board, depth);
  if (nodes != expected) {
    std::cerr << "FAIL fen=" << fen << " depth=" << depth << " got=" << nodes << " expected=" << expected
              << '\n';
    return false;
  }
  return true;
}

bool expect_fen_rejected(const std::string& fen) {
  Board board;
  board.set_startpos();
  if (board.set_fen(fen)) {
    std::cerr << "FAIL expected invalid fen to be rejected: " << fen << '\n';
    return false;
  }
  return true;
}

}  // namespace
}  // namespace engine

int main() {
  engine::init_attacks();

  bool ok = true;

  ok &= engine::expect_perft("startpos", 1, 20);
  ok &= engine::expect_perft("startpos", 2, 400);
  ok &= engine::expect_perft("startpos", 3, 8902);
  ok &= engine::expect_perft("startpos", 4, 197281);

  ok &= engine::expect_perft("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48);
  ok &= engine::expect_perft("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2,
                             2039);
  ok &= engine::expect_perft("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3,
                             97862);

  ok &= engine::expect_perft("r3k2r/8/8/8/8/8/8/4K2R w KQ - 0 1", 1, 15);
  ok &= engine::expect_perft("r3k2r/8/8/8/8/8/8/4K2R w KQ - 0 1", 2, 311);

  ok &= engine::expect_perft("rnbqkb1r/pppp1ppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", 1, 30);
  ok &= engine::expect_perft("rnbqkb1r/pppp1ppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", 2, 801);

  ok &= engine::expect_perft("8/2p5/1p6/8/5K2/8/k7/4r3 w - - 0 1", 1, 5);
  ok &= engine::expect_perft("8/2p5/1p6/8/5K2/8/k7/4r3 w - - 0 1", 3, 728);

  ok &= engine::expect_perft("8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 2, 126);

  ok &= engine::expect_perft("r3k2r/p1ppqpb1/bn2pnp1/3PN3/PP2P3/2N2Q1p/R2BPPBP/R3K2R b KQkq - 0 1", 1, 42);

  ok &= engine::expect_perft("8/2p5/8/8/8/8/6p1/4k2K w - - 0 1", 1, 3);

  ok &= engine::expect_perft("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 2, 1486);
  ok &= engine::expect_perft("n1bnk2r/p2p1ppp/2p4q/6b1/3PP3/2N2Q1p/PPP2PPP/R1B1KBNR b K - 2 9", 2, 1265);
  ok &= engine::expect_perft("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2", 2, 835);

  ok &= engine::expect_fen_rejected("r3k2r/Pppp1ppp/1b2p4/4P3/8/6P1/PPPPKP2/RNBQKBNR b KQkq - 0 1");
  ok &= engine::expect_fen_rejected("n1n5K1/6n1/8/8/8/8/PPPPPPPP/R3KN2r w Q - 0 1");

  if (ok) {
    std::cout << "All perft tests passed.\n";
    return 0;
  }
  return 1;
}
