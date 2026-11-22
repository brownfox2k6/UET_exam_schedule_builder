#pragma once

#include "problem_data.hpp"
#include "rooms/params.hpp"
#include "rooms/room_assignment.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {

/** Validates a fixed schedule and minimizes room count, then unused capacity. */
[[nodiscard]] auto assign_rooms(
    const ProblemData& problem,
    const Vector<int>& schedule,
    Params params
) -> RoomAssignment;

}  // namespace cetp::rooms
