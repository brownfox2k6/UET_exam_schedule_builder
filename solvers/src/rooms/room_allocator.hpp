#pragma once

#include <cstdint>

#include "problem_data.hpp"
#include "rooms/params.hpp"
#include "rooms/room_assignment.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {

/** Validates a fixed schedule and minimizes room count, extra locations, then unused capacity. */
[[nodiscard]] auto assign_rooms(
    const ProblemData& problem,
    const Vector<int>& schedule,
    Params params,
    uint64_t base_seed
) -> RoomAssignment;

}  // namespace cetp::rooms
