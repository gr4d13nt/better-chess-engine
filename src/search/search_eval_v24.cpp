#include "search_common.hpp"

#include "search_eval_bishop.hpp"
#include "search_eval_hanging.hpp"
#include "search_eval_knight.hpp"
#include "search_eval_layered.hpp"
#include "search_eval_mopup.hpp"
#include "search_eval_pawn_extended.hpp"
#include "search_eval_phase.hpp"
#include "search_eval_rook.hpp"
#include "search_eval_space.hpp"

namespace engine::search_common {
namespace {

constexpr int kTempoBonus = 18;

int evaluate_v24_piece(const Board& board) {
  int imp_mg_w = 0;
  int imp_eg_w = 0;
  int imp_mg_b = 0;
  int imp_eg_b = 0;
  evaluate_improved_split(board, imp_mg_w, imp_eg_w, imp_mg_b, imp_eg_b);

  int king_mg_w = 0;
  int king_eg_w = 0;
  int king_mg_b = 0;
  int king_eg_b = 0;
  evaluate_king_safety_split(board, king_mg_w, king_eg_w, king_mg_b, king_eg_b);

  int dbl_mg_w = 0;
  int dbl_eg_w = 0;
  int dbl_mg_b = 0;
  int dbl_eg_b = 0;
  evaluate_pawn_doubled_split(board, dbl_mg_w, dbl_eg_w, dbl_mg_b, dbl_eg_b);

  int ext_mg_w = 0;
  int ext_eg_w = 0;
  int ext_mg_b = 0;
  int ext_eg_b = 0;
  evaluate_pawn_extended_split(board, ext_mg_w, ext_eg_w, ext_mg_b, ext_eg_b);

  int space_mg_w = 0;
  int space_eg_w = 0;
  int space_mg_b = 0;
  int space_eg_b = 0;
  evaluate_space_split(board, space_mg_w, space_eg_w, space_mg_b, space_eg_b);

  int hang_mg_w = 0;
  int hang_eg_w = 0;
  int hang_mg_b = 0;
  int hang_eg_b = 0;
  evaluate_hanging_split(board, hang_mg_w, hang_eg_w, hang_mg_b, hang_eg_b);

  int knight_mg_w = 0;
  int knight_eg_w = 0;
  int knight_mg_b = 0;
  int knight_eg_b = 0;
  evaluate_knight_outposts_split(board, knight_mg_w, knight_eg_w, knight_mg_b, knight_eg_b);

  int rook_mg_w = 0;
  int rook_eg_w = 0;
  int rook_mg_b = 0;
  int rook_eg_b = 0;
  evaluate_rook_placement_split(board, rook_mg_w, rook_eg_w, rook_mg_b, rook_eg_b);

  int bishop_mg_w = 0;
  int bishop_eg_w = 0;
  int bishop_mg_b = 0;
  int bishop_eg_b = 0;
  evaluate_bishop_freedom_split(board, bishop_mg_w, bishop_eg_w, bishop_mg_b, bishop_eg_b);

  int pass_mg_w = 0;
  int pass_eg_w = 0;
  int pass_mg_b = 0;
  int pass_eg_b = 0;
  evaluate_passed_pawns_split(board, pass_mg_w, pass_eg_w, pass_mg_b, pass_eg_b);

  const int phase = game_phase(board);
  const int mg_net = (imp_mg_w + king_mg_w + dbl_mg_w + ext_mg_w + space_mg_w + hang_mg_w +
                      knight_mg_w + rook_mg_w + bishop_mg_w + pass_mg_w) -
                     (imp_mg_b + king_mg_b + dbl_mg_b + ext_mg_b + space_mg_b + hang_mg_b +
                      knight_mg_b + rook_mg_b + bishop_mg_b + pass_mg_b);
  const int eg_net = (imp_eg_w + king_eg_w + dbl_eg_w + ext_eg_w + space_eg_w + hang_eg_w +
                      knight_eg_w + rook_eg_w + bishop_eg_w + pass_eg_w) -
                     (imp_eg_b + king_eg_b + dbl_eg_b + ext_eg_b + space_eg_b + hang_eg_b +
                      knight_eg_b + rook_eg_b + bishop_eg_b + pass_eg_b);

  const int tempo = board.side_to_move() == Color::White ? kTempoBonus : -kTempoBonus;
  return taper_eval(mg_net, eg_net, phase) + tempo;
}

}  // namespace

int evaluate_v24(const Board& board) {
  return evaluate_v24_piece(board) + evaluate_mop_up_net(board);
}

}  // namespace engine::search_common
