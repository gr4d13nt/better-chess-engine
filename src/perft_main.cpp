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

}  // namespace
}  // namespace engine

int main(int argc, char** argv) {
  engine::init_attacks();

  engine::Board board;
  int depth = 5;
  std::string fen = "startpos";

  if (argc > 1 && std::string(argv[1]) == "divide") {
    if (argc > 3) {
      fen = argv[3];
    }
  } else {
    if (argc > 1) {
      depth = std::stoi(argv[1]);
    }
    if (argc > 2) {
      fen = argv[2];
    }
  }

  if (fen == "startpos") {
    board.set_startpos();
  } else if (!board.set_fen(fen)) {
    std::cerr << "Invalid FEN: " << fen << '\n';
    return 1;
  }

  if (argc > 1 && std::string(argv[1]) == "divide") {
    const int div_depth = argc > 2 ? std::stoi(argv[2]) : 3;
    engine::MoveList moves;
    engine::generate_legal_moves(board, moves);
    std::uint64_t total = 0;
    for (int i = 0; i < moves.count; ++i) {
      engine::Undo undo;
      board.make_move(moves.moves[i], undo);
      const auto n = engine::perft(board, div_depth - 1);
      board.unmake_move(moves.moves[i], undo);
      total += n;
      std::cout << engine::move_to_uci(moves.moves[i]) << ": " << n << '\n';
    }
    std::cout << "total: " << total << '\n';
    return 0;
  }

  const auto nodes = engine::perft(board, depth);
  std::cout << "perft(" << depth << ") = " << nodes << '\n';
  return 0;
}
