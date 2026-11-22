#pragma once

#include "rooms/params.hpp"
#include "slots/aco/params.hpp"
#include "slots/eval_params.hpp"
#include "slots/local_search/params.hpp"

namespace cetp {

/** Configuration for the default ACO, local search, and room assignment pipeline. */
struct Hyperparams {
  // NOLINTBEGIN(*-non-private-*)
  slots::EvalParams eval;
  slots::aco::Params aco;
  slots::local_search::Params ls;
  rooms::Params room_assignment;
  // NOLINTEND(*-non-private-*)

  explicit Hyperparams(
      slots::EvalParams eval_params = {},
      slots::aco::Params aco_params = {},
      slots::local_search::Params ls_params = {},
      rooms::Params room_params = {}
  )
      : eval(eval_params),
        aco(aco_params),
        ls(ls_params),
        room_assignment(room_params) {}
};

}  // namespace cetp
