#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/search.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  engine::init_attacks();

  engine::Board board;
  engine::SearchConfig cfg{};

  std::string fen = "startpos";
  // Usage:
  //   ./search_main <depth> [fen] [version] [movetime_ms]
  if (argc > 1) {
    cfg.depth = std::stoi(argv[1]);
  }
  if (argc > 2) {
    fen = argv[2];
  }
  if (argc > 3) {
    const int version = std::stoi(argv[3]);
    if (version == 16) {
      cfg.version = engine::EngineVersion::V16_SeeQsearch;
    } else if (version == 15) {
      cfg.version = engine::EngineVersion::V15_PiecePlacement;
    } else if (version == 14) {
      cfg.version = engine::EngineVersion::V14_RepetitionDraw;
    } else if (version == 13) {
      cfg.version = engine::EngineVersion::V13_MopUpEval;
    } else if (version == 12) {
      cfg.version = engine::EngineVersion::V12_CheckExtension;
    } else if (version == 11) {
      cfg.version = engine::EngineVersion::V11_TTHashMove;
    } else if (version == 10) {
      cfg.version = engine::EngineVersion::V10_TranspositionTable;
    } else if (version == 9) {
      cfg.version = engine::EngineVersion::V9_RookPlacement;
    } else if (version == 8) {
      cfg.version = engine::EngineVersion::V8_KnightOutposts;
    } else if (version == 7) {
      cfg.version = engine::EngineVersion::V7_PhasedEval;
    } else if (version == 6) {
      cfg.version = engine::EngineVersion::V6_PawnKingEval;
    } else if (version == 5) {
      cfg.version = engine::EngineVersion::V5_MoveOrdering;
    } else if (version == 4) {
      cfg.version = engine::EngineVersion::V4_Quiescence;
    } else if (version == 3) {
      cfg.version = engine::EngineVersion::V3_ImprovedEval;
    } else if (version == 2) {
      cfg.version = engine::EngineVersion::V2_AlphaBeta;
    } else {
      cfg.version = engine::EngineVersion::V1_NoPruning;
    }
  }
  if (argc > 4) {
    cfg.movetime_ms = std::stoi(argv[4]);
  }

  if (fen == "startpos") {
    board.set_startpos();
  } else if (!board.set_fen(fen)) {
    std::cerr << "Invalid FEN: " << fen << '\n';
    return 1;
  }

  const engine::SearchResult result = engine::search_best_move(board, cfg);
  if (!result.has_move) {
    std::cout << "nomove score=" << result.score << " nodes=" << result.nodes << '\n';
    return 0;
  }

  std::cout << "bestmove " << engine::move_to_uci(result.best_move) << '\n';
  std::cout << "version " << static_cast<int>(cfg.version) << '\n';
  std::cout << "movetime_ms " << cfg.movetime_ms << '\n';
  std::cout << "score " << result.score << '\n';
  std::cout << "nodes " << result.nodes << '\n';
  return 0;
}
