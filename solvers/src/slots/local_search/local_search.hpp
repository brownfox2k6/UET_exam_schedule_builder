#pragma once

#include <cstdint>

#include "problem_data.hpp"
#include "slots/eval_params.hpp"
#include "slots/local_search/params.hpp"
#include "slots/slot_result.hpp"
#include "utils/vector.hpp"

namespace cetp::slots::local_search {

/** Validates the initial schedule and improves it according to the evaluation parameters. */
[[nodiscard]] auto improve(
    const ProblemData& problem,
    const Vector<int>& schedule,
    Params params,
    EvalParams eval_params,
    uint64_t base_seed
) -> SlotResult;

}  // namespace cetp::slots::local_search
