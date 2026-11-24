#pragma once

#include <cstdint>

#include <ortools/sat/cp_model.h>

#include "problem_data.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {

/**
 * Declarative OR-Tools CP-SAT model for a single independent time slot.
 *
 * Holds every boolean decision variable and the three linear objective expressions optimized in
 * lexicographic order:
 *   1. `room_count`      - total number of rooms opened in this slot,
 *   2. `extra_locations` - sum over multi-section exams of (distinct campuses used - 1),
 *   3. `total_capacity`  - total seats opened (minimizing unused seats).
 */
struct SlotModel {
  // CpModelBuilder collects variables, constraints, and objectives; nothing is solved until
  // `SolveCpModel(model.Build(), &solver)` is invoked.
  operations_research::sat::CpModelBuilder model;

  // All boolean variables in the model (room choices + campus indicators), stored together so
  // the solver can pass a complete warm-start solution ("hint") from one phase to the next.
  Vector<operations_research::sat::BoolVar> all_variables;

  // Decision variables for eligible (section, room) pairs, ordered by `slot_sections` and each
  // section's `feasible_rooms_by_section` row. Value 1 means that section uses that room.
  Vector<operations_research::sat::BoolVar> room_choices;

  // LinearExpr is a symbolic linear sum of variables (e.g. `40*x0 + 30*x1 + ...`).
  operations_research::sat::LinearExpr room_count;
  operations_research::sat::LinearExpr extra_locations;
  operations_research::sat::LinearExpr total_capacity;

  // Theoretical minimum of `extra_locations` forced by single-campus sections (usually 0).
  // When phase 1 already hits this bound, phase 2 can be skipped completely.
  int64_t extra_locations_lower_bound = 0;
  bool has_variable_exam_locations = false;
};

/**
 * Builds the complete CP-SAT model (variables, hard constraints, and objectives) for one
 * non-empty time slot containing `slot_exams` and their expanded `slot_sections`.
 */
[[nodiscard]] auto build_slot_model(
    const ProblemData& problem,
    Span<const int> slot_exams,
    Span<const int> slot_sections
) -> SlotModel;

}  // namespace cetp::rooms
