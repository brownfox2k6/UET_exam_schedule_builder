#pragma once

#include <cstdint>

#include "evaluation/params.hpp"
#include "problem_data.hpp"
#include "slots/aco/params.hpp"
#include "slots/slot_result.hpp"

namespace cetp::slots::aco {

/** Reports progress after each one-based iteration. */
[[nodiscard]] auto solve(
    const ProblemData& problem,
    Params params,
    evaluation::Params eval_params,
    uint64_t base_seed,
    const SlotProgressCallback& callback
) -> SlotResult;

}  // namespace cetp::slots::aco
