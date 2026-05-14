#pragma once

#include <utility>
#include <vector>

struct Exam {
  int student_count;
  std::vector<int> feasible_slots;
  std::vector<int> feasible_rooms;
  std::vector<int> feasible_proctors;

  Exam() = default;

  Exam(
    int _student_count,
    std::vector<int> _feasible_slots,
    std::vector<int> _feasible_rooms,
    std::vector<int> _feasible_proctors
  ) : student_count(_student_count),
      feasible_slots(std::move(_feasible_slots)),
      feasible_rooms(std::move(_feasible_rooms)),
      feasible_proctors(std::move(_feasible_proctors))
  {}
};