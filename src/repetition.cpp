#include "engine/repetition.hpp"

#include "engine/zobrist.hpp"

namespace engine {

std::vector<std::uint64_t> repetition_hashes_from_fens(const std::vector<std::string>& fens) {
  std::vector<std::uint64_t> out;
  out.reserve(fens.size());
  Board board;
  for (const std::string& fen : fens) {
    if (fen == "startpos") {
      board.set_startpos();
    } else if (!board.set_fen(fen)) {
      continue;
    }
    out.push_back(board.zobrist());
  }
  return out;
}

}  // namespace engine
