#include "search_common.hpp"

#include "search_eval_layered.hpp"
#include "search_eval_phase.hpp"

namespace engine::search_common {

namespace {

constexpr int kTempoBonus = 18;

}  // namespace

int evaluate_v7(const Board& board) {
  int imp_mg_w = 0;
  int imp_eg_w = 0;
  int imp_mg_b = 0;
  int imp_eg_b = 0;
  evaluate_improved_split(board, imp_mg_w, imp_eg_w, imp_mg_b, imp_eg_b);

  int lay_mg_w = 0;
  int lay_eg_w = 0;
  int lay_mg_b = 0;
  int lay_eg_b = 0;
  evaluate_layered_split(board, lay_mg_w, lay_eg_w, lay_mg_b, lay_eg_b);

  const int phase = game_phase(board);
  const int mg_net = (imp_mg_w + lay_mg_w) - (imp_mg_b + lay_mg_b);
  const int eg_net = (imp_eg_w + lay_eg_w) - (imp_eg_b + lay_eg_b);

  const int tempo = board.side_to_move() == Color::White ? kTempoBonus : -kTempoBonus;
  return taper_eval(mg_net, eg_net, phase) + tempo;
}

}  // namespace engine::search_common
