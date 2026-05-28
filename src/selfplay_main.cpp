#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"

#include <iostream>
#include <string>

namespace {

engine::EngineVersion parse_version(int v) {
  return (v == 2) ? engine::EngineVersion::V2_AlphaBeta : engine::EngineVersion::V1_NoPruning;
}

const char* version_name(engine::EngineVersion v) {
  return v == engine::EngineVersion::V2_AlphaBeta ? "v2-alpha-beta" : "v1-no-pruning";
}

}  // namespace

int main(int argc, char** argv) {
  engine::init_attacks();

  // Usage:
  //   ./selfplay_main <white_version> <black_version> <depth> <max_plies> [movetime_ms] [fen]
  // Example:
  //   ./selfplay_main 1 2 3 120 startpos
  int white_version = 1;
  int black_version = 2;
  int depth = 3;
  int max_plies = 120;
  int movetime_ms = 0;
  std::string fen = "startpos";

  if (argc > 1) {
    white_version = std::stoi(argv[1]);
  }
  if (argc > 2) {
    black_version = std::stoi(argv[2]);
  }
  if (argc > 3) {
    depth = std::stoi(argv[3]);
  }
  if (argc > 4) {
    max_plies = std::stoi(argv[4]);
  }
  if (argc > 5) {
    movetime_ms = std::stoi(argv[5]);
  }
  if (argc > 6) {
    fen = argv[6];
  }

  engine::Board board;
  if (fen == "startpos") {
    board.set_startpos();
  } else if (!board.set_fen(fen)) {
    std::cerr << "Invalid FEN: " << fen << '\n';
    return 1;
  }

  const auto white = parse_version(white_version);
  const auto black = parse_version(black_version);

  std::cout << "selfplay white=" << version_name(white) << " black=" << version_name(black)
            << " depth=" << depth << " max_plies=" << max_plies
            << " movetime_ms=" << movetime_ms << '\n';

  int plies = 0;
  while (plies < max_plies) {
    engine::MoveList legal;
    engine::generate_legal_moves(board, legal);
    if (legal.count == 0) {
      if (board.in_check(board.side_to_move())) {
        std::cout << "result "
                  << (board.side_to_move() == engine::Color::White ? "0-1" : "1-0")
                  << " checkmate\n";
      } else {
        std::cout << "result 1/2-1/2 stalemate\n";
      }
      return 0;
    }

    engine::SearchConfig cfg{};
    cfg.depth = depth;
    cfg.version = (board.side_to_move() == engine::Color::White) ? white : black;
    cfg.movetime_ms = movetime_ms;

    engine::SearchResult result = engine::search_best_move(board, cfg);
    if (!result.has_move) {
      std::cout << "result 1/2-1/2 nomove\n";
      return 0;
    }

    std::cout << "ply " << (plies + 1) << " "
              << (board.side_to_move() == engine::Color::White ? "W" : "B") << " "
              << version_name(cfg.version) << " move=" << engine::move_to_uci(result.best_move)
              << " score=" << result.score << " nodes=" << result.nodes << '\n';

    engine::Undo undo;
    board.make_move(result.best_move, undo);
    plies++;
  }

  std::cout << "result 1/2-1/2 max-plies\n";
  std::cout << "final-fen " << board.fen() << '\n';
  return 0;
}
