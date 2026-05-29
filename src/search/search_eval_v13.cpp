#include "search_common.hpp"

#include "search_eval_mopup.hpp"

namespace engine::search_common {

int evaluate_v13(const Board& board) {
  return evaluate_v7(board) + evaluate_mop_up_net(board);
}

}  // namespace engine::search_common
