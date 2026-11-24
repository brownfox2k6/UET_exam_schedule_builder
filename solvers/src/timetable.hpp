#pragma once

#include <cstdint>
#include <optional>

#include "hyperparameters.hpp"
#include "problem_data.hpp"
#include "rooms/params.hpp"
#include "rooms/room_assignment.hpp"
#include "slots/slot_result.hpp"
#include "utils/vector.hpp"

namespace cetp {

struct TimetableResult {
  slots::SlotResult slots;
  std::optional<rooms::RoomAssignment> rooms;
};

/** Completes the result of any slot search with room allocation when a schedule exists. */
[[nodiscard]] auto complete_timetable(
    const ProblemData& problem,
    slots::SlotResult slot_result,
    rooms::Params room_params,
    uint64_t base_seed
) -> TimetableResult;

[[nodiscard]] auto solve_timetable(
    const ProblemData& problem,
    Hyperparams hyperparams,
    uint64_t base_seed,
    const slots::SlotProgressCallback& callback
) -> TimetableResult;

/** Checks a complete schedule against the problem's hard slot constraints. */
void validate_schedule(const ProblemData& problem, const Vector<int>& schedule);

}  // namespace cetp
