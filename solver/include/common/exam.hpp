#pragma once

#include <cstdlib>
#include <set>
#include <string>
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
      code(_code),
      credits(_credits),
      student_count(_students.size()),
      students(std::move(_students)),
      feasible_slots(std::move(_feasible_slots)),
      feasible_rooms(std::move(_feasible_rooms)),
      feasible_proctors(std::move(_feasible_proctors))
  {
#ifndef NDEBUG
    utils::panic_if(code.empty(), "Exam code cannot be an empty string");
    utils::panic_if(credits <= 0, "Exam '{}' must have positive credits (got: {})", code, credits);
    utils::panic_if(students.empty(), "Exam '{}' has 0 registered students", code);
    std::set<std::string> unique_checker;
    for (const std::string& s : students) {
      utils::panic_if(s.empty(), "Exam '{}' contains an empty student ID", code);
      auto [it, is_inserted] = unique_checker.emplace(s);
      utils::panic_if(!is_inserted, "'students' of exam '{}' contains duplicate ID: '{}'", code, s);
    }
    utils::panic_if(feasible_slots.empty(), "'feasible_slots' of exam '{}' is empty", code);
    utils::panic_if(feasible_rooms.empty(), "'feasible_rooms' of exam '{}' is empty", code);
    utils::panic_if(feasible_proctors.empty(), "'feasible_proctors' of exam '{}' is empty", code);
    for (int slot : feasible_slots) {
      utils::panic_if(slot < 0, "'feasible_slots' of exam '{}' has negative value ({})", code, slot);
    }
    for (int room : feasible_rooms) {
      utils::panic_if(room < 0, "'feasible_rooms' of exam '{}' has negative value ({})", code, room);
    }
    for (int proctor : feasible_proctors) {
      utils::panic_if(proctor < 0, "'feasible_proctors' of exam '{}' has negative value ({})", code, proctor);
    }
#endif // NDEBUG
  }
}; // struct Exam

} // namespace common