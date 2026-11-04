#include "common/solution.hpp"

#include <cstddef>

#include "common/problem_data.hpp"

namespace common {

Solution::Solution(const ProblemData& problem_data)
    : slot_assignment(problem_data), room_assignment(problem_data) {}

void Solution::reset() {
  slot_assignment.reset();
  room_assignment.reset();
  penalty = 0.0;
}

}  // namespace common
