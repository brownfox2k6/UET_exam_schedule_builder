#include "rooms/slot_model.hpp"

#include <algorithm>

#include <ortools/sat/cp_model.h>

#include "problem_data.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {
namespace {

namespace sat = operations_research::sat;

// Maps one eligible (section, room) candidate in this slot to its boolean decision variable.
// Sorting these pairs by `room` lets us build exclusive-room constraints without a dense matrix.
struct RoomUse {
  int room;
  int choice;
};

// Stores a section's campus/location choices within this slot's CP-SAT model.
// - If all eligible rooms share one campus (`fixed_location >= 0`), no extra variable is created.
// - Otherwise, `location_variables[index] == 1` iff the section is placed at `location_ids[index]`.
struct SectionLocationVariables {
  int fixed_location = -1;
  Vector<int> location_ids;
  Vector<sat::BoolVar> location_variables;
};

auto collect_unique_locations(const ProblemData& problem, Span<const int> rooms) -> Vector<int> {
  Vector<int> location_ids;
  location_ids.reserve(rooms.size());
  for (int room : rooms) {
    location_ids.push_back(problem.room_location_ids[room]);
  }
  std::ranges::sort(location_ids);
  const auto unique_end = std::ranges::unique(location_ids).begin();
  location_ids.erase(unique_end, location_ids.end());
  return location_ids;
}

auto find_section_location_variable(
    const SectionLocationVariables& section_location,
    int location_id
) -> sat::BoolVar {
  const auto iterator = std::ranges::lower_bound(section_location.location_ids, location_id);
  return section_location.location_variables[int(iterator - section_location.location_ids.begin())];
}

// Creates hard single-location decision variables for one section: all rooms assigned to a
// section must belong to the same campus/building.
auto add_section_location_constraint(
    const ProblemData& problem,
    Span<const int> eligible_rooms,
    SlotModel& slot_model
) -> SectionLocationVariables {
  const Vector<int> location_ids = collect_unique_locations(problem, eligible_rooms);
  SectionLocationVariables section_location;
  if (location_ids.size() == 1) {
    section_location.fixed_location = location_ids[0];
    return section_location;
  }

  section_location.location_ids = location_ids;
  section_location.location_variables.reserve(location_ids.size());
  for (int location_index = 0; location_index < location_ids.size(); ++location_index) {
    const sat::BoolVar location_variable = slot_model.model.NewBoolVar();
    section_location.location_variables.push_back(location_variable);
    slot_model.all_variables.push_back(location_variable);
  }
  // AddExactlyOne constrains sum(variables) == 1 so the section picks exactly one campus.
  slot_model.model.AddExactlyOne(section_location.location_variables.std_vector());
  return section_location;
}

// Creates (section, room) choice variables, capacity constraint, and campus implications.
void add_section_room_constraints(
    const ProblemData& problem,
    int section,
    const SectionLocationVariables& section_location,
    SlotModel& slot_model,
    Vector<RoomUse>& uses
) {
  sat::LinearExpr section_capacity;
  for (int room : problem.feasible_rooms_by_section[section]) {
    // NewBoolVar creates a 0/1 variable: 1 means `section` uses `room`.
    const sat::BoolVar choice = slot_model.model.NewBoolVar();
    const int choice_index = slot_model.room_choices.size();
    slot_model.room_choices.push_back(choice);
    slot_model.all_variables.push_back(choice);
    uses.push_back({.room = room, .choice = choice_index});

    const int seats = problem.room_capacities[room];
    section_capacity += seats * choice;
    slot_model.total_capacity += seats * choice;
    slot_model.room_count += choice;

    // AddImplication(choice, location_variable) enforces: if this section uses `room`, then the
    // section must be placed at `room`'s campus (forbidding rooms from unchosen campuses).
    if (section_location.fixed_location < 0) {
      const int room_location_id = problem.room_location_ids[room];
      slot_model.model.AddImplication(
          choice,
          find_section_location_variable(section_location, room_location_id)
      );
    }
  }

  // AddGreaterOrEqual enforces sum(seats * choice) >= section student count.
  slot_model.model.AddGreaterOrEqual(section_capacity, problem.section_student_counts[section]);
}

// Enforces hard exclusivity: each room in this slot may be assigned to at most one section.
void add_exclusive_room_constraints(SlotModel& slot_model, Vector<RoomUse>& uses) {
  std::ranges::sort(uses, [](const RoomUse& left, const RoomUse& right) -> bool {
    return left.room < right.room || (left.room == right.room && left.choice < right.choice);
  });

  Vector<sat::BoolVar> room_group;
  for (int begin = 0; begin < uses.size();) {
    int end = begin;
    room_group.clear();
    while (end < uses.size() && uses[end].room == uses[begin].room) {
      room_group.push_back(slot_model.room_choices[uses[end].choice]);
      ++end;
    }
    if (room_group.size() > 1) {
      // AddAtMostOne constrains sum(variables) <= 1 so no two sections share the same room.
      slot_model.model.AddAtMostOne(room_group.std_vector());
    }
    begin = end;
  }
}

auto collect_exam_candidate_locations(Span<const SectionLocationVariables> exam_section_locations)
    -> Vector<int> {
  Vector<int> candidate_locations;
  for (const SectionLocationVariables& section_location : exam_section_locations) {
    if (section_location.fixed_location >= 0) {
      candidate_locations.push_back(section_location.fixed_location);
      continue;
    }
    for (int location_id : section_location.location_ids) {
      candidate_locations.push_back(location_id);
    }
  }
  std::ranges::sort(candidate_locations);
  const auto unique_end = std::ranges::unique(candidate_locations).begin();
  candidate_locations.erase(unique_end, candidate_locations.end());
  return candidate_locations;
}

// Adds one campus's 0/1 contribution to `used_locations_count` for a multi-section exam.
// Returns true if `location_id` is unconditionally forced by at least one single-campus section.
auto add_exam_location_indicator(
    int location_id,
    Span<const SectionLocationVariables> exam_section_locations,
    SlotModel& slot_model,
    sat::LinearExpr& used_locations_count
) -> bool {
  Vector<sat::BoolVar> indicators;
  for (const SectionLocationVariables& section_location : exam_section_locations) {
    if (section_location.fixed_location == location_id) {
      used_locations_count += 1;
      return true;
    }
    if (section_location.fixed_location < 0
        && std::ranges::binary_search(section_location.location_ids, location_id)) {
      indicators.push_back(find_section_location_variable(section_location, location_id));
    }
  }

  slot_model.has_variable_exam_locations = true;
  if (indicators.size() == 1) {
    used_locations_count += indicators[0];
    return false;
  }

  // Create `exam_uses_location == OR(indicators)` using two boolean constraints:
  //   1. `indicator => exam_uses_location` (AddImplication): any section there sets it to 1.
  //   2. `exam_uses_location => OR(indicators)` (AddBoolOr + OnlyEnforceIf): if no section is
  //      placed there, it MUST be 0 (ensuring exact objective readouts after phase 1).
  const sat::BoolVar exam_uses_location = slot_model.model.NewBoolVar();
  slot_model.all_variables.push_back(exam_uses_location);
  for (const sat::BoolVar indicator : indicators) {
    slot_model.model.AddImplication(indicator, exam_uses_location);
  }
  slot_model.model.AddBoolOr(indicators.std_vector()).OnlyEnforceIf(exam_uses_location);
  used_locations_count += exam_uses_location;
  return false;
}

// Adds soft penalty `(distinct campuses used by this exam) - 1` to `slot_model.extra_locations`.
void add_single_exam_location_spread(
    Span<const SectionLocationVariables> exam_section_locations,
    SlotModel& slot_model
) {
  if (exam_section_locations.size() <= 1) {
    return;
  }
  const Vector<int> candidate_locations = collect_exam_candidate_locations(exam_section_locations);
  if (candidate_locations.size() <= 1) {
    return;
  }

  sat::LinearExpr used_locations_count;
  int fixed_location_count = 0;
  for (int location_id : candidate_locations) {
    if (add_exam_location_indicator(
            location_id,
            exam_section_locations,
            slot_model,
            used_locations_count
        )) {
      ++fixed_location_count;
    }
  }

  slot_model.extra_locations += used_locations_count - 1;
  if (fixed_location_count > 1) {
    slot_model.extra_locations_lower_bound += fixed_location_count - 1;
  }
}

void add_exam_location_spread(
    const ProblemData& problem,
    Span<const int> slot_exams,
    const Vector<SectionLocationVariables>& section_location_variables,
    SlotModel& slot_model
) {
  const Span<const SectionLocationVariables> all_section_locations{section_location_variables};
  int local_section_offset = 0;
  for (int exam : slot_exams) {
    const int exam_section_count = int(problem.sections_of_exam(exam).size());
    add_single_exam_location_spread(
        all_section_locations.subspan(local_section_offset, exam_section_count),
        slot_model
    );
    local_section_offset += exam_section_count;
  }
}

}  // namespace

auto build_slot_model(
    const ProblemData& problem,
    Span<const int> slot_exams,
    Span<const int> slot_sections
) -> SlotModel {
  SlotModel slot_model;

  int total_choices = 0;
  for (int section : slot_sections) {
    total_choices += problem.feasible_rooms_by_section[section].size();
  }
  slot_model.room_choices.reserve(total_choices);
  slot_model.all_variables.reserve(total_choices);

  Vector<RoomUse> uses;
  uses.reserve(total_choices);

  Vector<SectionLocationVariables> section_location_variables;
  section_location_variables.reserve(slot_sections.size());

  for (int section : slot_sections) {
    const Span<const int> eligible_rooms = problem.feasible_rooms_by_section[section];
    const SectionLocationVariables section_location =
        add_section_location_constraint(problem, eligible_rooms, slot_model);
    add_section_room_constraints(problem, section, section_location, slot_model, uses);
    section_location_variables.push_back(section_location);
  }

  add_exclusive_room_constraints(slot_model, uses);
  add_exam_location_spread(problem, slot_exams, section_location_variables, slot_model);

  return slot_model;
}

}  // namespace cetp::rooms
