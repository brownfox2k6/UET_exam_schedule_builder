#pragma once

#include "problem_data.hpp"
#include "slots/slot_assignment.hpp"

namespace cetp::slots {

/** Returns the most constrained unscheduled exam or -1 for a complete schedule. */
[[nodiscard]] auto select_next_exam(const ProblemData& problem, const SlotAssignment& candidate)
    -> int;

}  // namespace cetp::slots
