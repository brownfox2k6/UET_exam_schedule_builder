#pragma once

#include <cstdlib>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "utils/assert.hpp"

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
  std::vector<int> feasible_proctors;

  Exam() = default;

  Exam(
    std::string _code,
    int _credits,
    std::vector<std::string> _students,
    std::vector<int> _feasible_slots,
    std::vector<int> _feasible_rooms,
    std::vector<int> _feasible_proctors
  ) :
      code(std::move(_code)),
      credits(_credits),
      student_count(_students.size()),
      students(std::move(_students)),
      feasible_slots(std::move(_feasible_slots)),
      feasible_rooms(std::move(_feasible_rooms)),
      feasible_proctors(std::move(_feasible_proctors))
  {
#ifndef NDEBUG
    utils::panic_if(code.empty(), "Exam code cannot be an empty string");
    utils::panic_if(credits <= 0, "Exam '{}': Must have positive credits (got: {})", code, credits);
    utils::panic_if(students.empty(), "Exam '{}': No registered students", code);
    std::unordered_set<std::string> student_unique_checker;
    for (const std::string& s : students) {
      utils::panic_if(s.empty(), "Exam '{}': `students` contains an empty ID", code);
      utils::panic_if(!student_unique_checker.emplace(s).second,
                      "Exam '{}': 'students' contains duplicate ID: '{}'", code, s);
    }
    utils::panic_if(feasible_slots.empty(), "Exam '{}': 'feasible_slots' is empty", code);
    utils::panic_if(feasible_rooms.empty(), "Exam '{}': 'feasible_rooms' is empty", code);
    utils::panic_if(feasible_proctors.empty(), "Exam '{}': 'feasible_proctors' is empty", code);
    std::unordered_set<int> unique_checker;
    for (int slot : feasible_slots) {
      utils::panic_if(slot < 0, "Exam '{}': 'feasible_slots' has a negative value ({})", code, slot);
      utils::panic_if(!unique_checker.emplace(slot).second,
                      "Exam '{}': 'feasible_slots' has duplicate values ({})", code, slot);
    }
    unique_checker.clear();
    for (int room : feasible_rooms) {
      utils::panic_if(room < 0, "Exam '{}': 'feasible_rooms' has a negative value ({})", code, room);
      utils::panic_if(!unique_checker.emplace(room).second,
                      "Exam '{}': 'feasible_rooms' has duplicate values ({})", code, room);
    }
    unique_checker.clear();
    for (int proctor : feasible_proctors) {
      utils::panic_if(proctor < 0, "Exam '{}': 'feasible_proctors' has a negative value ({})", code, proctor);
      utils::panic_if(!unique_checker.emplace(proctor).second,
                      "Exam '{}': 'feasible_proctors' has duplicate values ({})", code, proctor);
    }
#endif // NDEBUG
  }

}; // struct Exam

} // namespace common