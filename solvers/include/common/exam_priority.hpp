#pragma once

#include <climits>
#include <compare>

namespace common {

/**
 * Implementation based on "Multi-Neighbourhood Simulated Annealing for the ITC-2007 Capacitated
 * Examination Timetabling Problem" by David van Bulck et al. (2025)
 * @see 10.1007/s10951-023-00799-1
 *
 * The following ordering criteria are applied in sequence:
 * (1) Saturation degree - number of feasible slots remaining (fewest first, dynamically updated
 *     after each assignment).
 * (2) Conflict degree - number of conflicting exams (largest first).
 * (3) Weighted conflict degree - total shared students with conflicting exams (largest first).
 * (4) Student count - number of enrolled students (largest first).
 */
struct ExamPriority {
  // NOLINTBEGIN(*-non-private-*)
  int saturation_degree{INT_MIN};
  int conflict_degree{INT_MIN};
  int weighted_conflict_degree{INT_MIN};
  int student_count{INT_MIN};
  // NOLINTEND(*-non-private-*)

  auto operator<=>(const ExamPriority&) const -> std::strong_ordering = default;
};  // struct ExamPriority

}  // namespace common
