#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

engine::EngineVersion parse_version(int v) {
  if (v == 14) {
    return engine::EngineVersion::V14_RepetitionDraw;
  }
  if (v == 13) {
    return engine::EngineVersion::V13_MopUpEval;
  }
  if (v == 12) {
    return engine::EngineVersion::V12_CheckExtension;
  }
  if (v == 11) {
    return engine::EngineVersion::V11_TTHashMove;
  }
  if (v == 10) {
    return engine::EngineVersion::V10_TranspositionTable;
  }
  if (v == 9) {
    return engine::EngineVersion::V9_RookPlacement;
  }
  if (v == 8) {
    return engine::EngineVersion::V8_KnightOutposts;
  }
  if (v == 7) {
    return engine::EngineVersion::V7_PhasedEval;
  }
  if (v == 6) {
    return engine::EngineVersion::V6_PawnKingEval;
  }
  if (v == 5) {
    return engine::EngineVersion::V5_MoveOrdering;
  }
  if (v == 4) {
    return engine::EngineVersion::V4_Quiescence;
  }
  if (v == 3) {
    return engine::EngineVersion::V3_ImprovedEval;
  }
  return (v == 2) ? engine::EngineVersion::V2_AlphaBeta : engine::EngineVersion::V1_NoPruning;
}

const char* version_name(engine::EngineVersion v) {
  if (v == engine::EngineVersion::V14_RepetitionDraw) {
    return "v14-repetition-draw";
  }
  if (v == engine::EngineVersion::V13_MopUpEval) {
    return "v13-mop-up-eval";
  }
  if (v == engine::EngineVersion::V12_CheckExtension) {
    return "v12-check-extension";
  }
  if (v == engine::EngineVersion::V11_TTHashMove) {
    return "v11-tt-hash-move";
  }
  if (v == engine::EngineVersion::V10_TranspositionTable) {
    return "v10-transposition-table";
  }
  if (v == engine::EngineVersion::V9_RookPlacement) {
    return "v9-rook-placement";
  }
  if (v == engine::EngineVersion::V8_KnightOutposts) {
    return "v8-knight-outposts";
  }
  if (v == engine::EngineVersion::V7_PhasedEval) {
    return "v7-phased-eval";
  }
  if (v == engine::EngineVersion::V6_PawnKingEval) {
    return "v6-pawn-king-eval";
  }
  if (v == engine::EngineVersion::V5_MoveOrdering) {
    return "v5-move-ordering";
  }
  if (v == engine::EngineVersion::V4_Quiescence) {
    return "v4-quiescence";
  }
  if (v == engine::EngineVersion::V3_ImprovedEval) {
    return "v3-improved-eval";
  }
  return v == engine::EngineVersion::V2_AlphaBeta ? "v2-alpha-beta" : "v1-no-pruning";
}

int count_hash(const std::vector<std::uint64_t>& hashes, std::uint64_t key) {
  int count = 0;
  for (std::uint64_t h : hashes) {
    if (h == key) {
      ++count;
    }
  }
  return count;
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

  std::vector<std::uint64_t> repetition_hashes;
  repetition_hashes.push_back(board.zobrist());

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

    cfg.repetition_history = repetition_hashes;

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
    repetition_hashes.push_back(board.zobrist());
    if (count_hash(repetition_hashes, board.zobrist()) >= 3) {
      std::cout << "result 1/2-1/2 threefold-repetition\n";
      return 0;
    }
    plies++;
  }

  std::cout << "result 1/2-1/2 max-plies\n";
  std::cout << "final-fen " << board.fen() << '\n';
  return 0;
}
