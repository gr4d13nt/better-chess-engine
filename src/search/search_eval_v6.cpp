#include "search_common.hpp"

#include "search_eval_layered.hpp"
#include "search_eval_phase.hpp"

namespace engine::search_common {

int evaluate_pawn_structure_net(const Board& board) {
  int mg_w = 0;
  int eg_w = 0;
  int mg_b = 0;
  int eg_b = 0;
  evaluate_pawn_structure_split(board, mg_w, eg_w, mg_b, eg_b);
  const int phase = game_phase(board);
  return taper_eval(mg_w - mg_b, eg_w - eg_b, phase);
}

int evaluate_king_safety_net(const Board& board) {
  int mg_w = 0;
  int eg_w = 0;
  int mg_b = 0;
  int eg_b = 0;
  evaluate_king_safety_split(board, mg_w, eg_w, mg_b, eg_b);
  const int phase = game_phase(board);
  return taper_eval(mg_w - mg_b, eg_w - eg_b, phase);
}

int evaluate_v6(const Board& board) {
  int mg_w = 0;
  int eg_w = 0;
  int mg_b = 0;
  int eg_b = 0;
  evaluate_layered_split(board, mg_w, eg_w, mg_b, eg_b);
  const int phase = game_phase(board);
  return evaluate_improved(board) + taper_eval(mg_w - mg_b, eg_w - eg_b, phase);
}

}  // namespace engine::search_common
