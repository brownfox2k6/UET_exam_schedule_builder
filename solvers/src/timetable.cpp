#include "timetable.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

#include "hyperparameters.hpp"
#include "problem_data.hpp"
#include "rooms/params.hpp"
#include "rooms/room_allocator.hpp"
#include "slots/aco/ant_colony.hpp"
#include "slots/local_search/local_search.hpp"
#include "slots/slot_result.hpp"
#include "utils/vector.hpp"

namespace cetp {

auto complete_timetable(
    const ProblemData& problem,
    slots::SlotResult slot_result,
    rooms::Params room_params,
    uint64_t base_seed
) -> TimetableResult {
  if (!slot_result.found()) {
    return {.slots = std::move(slot_result), .rooms = std::nullopt};
  }
  auto room_assignment =
      rooms::assign_rooms(problem, slot_result.schedule, room_params, base_seed);
  return {.slots = std::move(slot_result), .rooms = std::move(room_assignment)};
}

auto solve_timetable(
    const ProblemData& problem,
    Hyperparams hyperparams,
    uint64_t base_seed,
    const slots::SlotProgressCallback& callback
) -> TimetableResult {
  auto slot_result =
      slots::aco::solve(problem, hyperparams.aco, hyperparams.eval, base_seed, callback);
  if (slot_result.found()) {
    slot_result = slots::local_search::improve(
        problem,
        slot_result.schedule,
        hyperparams.ls,
        hyperparams.eval,
        base_seed
    );
  }
  return complete_timetable(
      problem,
      std::move(slot_result),
      hyperparams.room_assignment,
      base_seed
  );
}

void validate_schedule(const ProblemData& problem, const Vector<int>& schedule) {
  if (schedule.size() != problem.num_exams) {
    throw std::invalid_argument(
        std::format(
            "Schedule length ({}) must match the number of exams ({})",
            schedule.size(),
            problem.num_exams
        )
    );
  }

  for (int exam : problem.exams()) {
    const int slot = schedule[exam];
    const auto feasible = problem.feasible_slots_by_exam[exam];
    if (!std::ranges::binary_search(feasible, slot)) {
      throw std::invalid_argument(
          std::format(
              "Exam '{}' assigned to invalid or infeasible slot {}",
              problem.exam_codes[exam],
              slot
          )
      );
    }

    for (int other : problem.conflicts_csr[exam].indices) {
      if (other > exam && slot == schedule[other]) {
        throw std::invalid_argument(
            std::format(
                "Exams '{}' and '{}' conflict in slot {}",
                problem.exam_codes[exam],
                problem.exam_codes[other],
                slot
            )
        );
      }
    }
  }
}

}  // namespace cetp
