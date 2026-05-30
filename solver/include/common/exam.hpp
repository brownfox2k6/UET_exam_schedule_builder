#pragma once

#include <vector>

namespace common {

/**
 * @brief Represents an exam entity in the Examination Timetabling Problem.
 */
struct Exam {

  // --- Heuristic Weights & Metadata ---
  std::string code;  // Exam code (e.g. "INT1008 1")
  int credits = 1;
  int student_count = 0;

  // --- Core Operational Constraints ---
  std::vector<std::string> students;
  std::vector<int> feasible_slots;
  std::vector<int> feasible_rooms;

  Exam(
    std::string _code,
    int _credits,
    std::vector<std::string> _students,
    std::vector<int> _feasible_slots,
    std::vector<int> _feasible_rooms
  );

}; // struct Exam

} // namespace common