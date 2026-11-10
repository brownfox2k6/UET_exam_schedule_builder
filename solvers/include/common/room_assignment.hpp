#pragma once

#include "common/problem_data.hpp"

namespace common {

struct RoomAssignment {
  RoomAssignment(const ProblemData& problem_data);

  void reset();
};  // struct RoomAssignment

}  // namespace common
