#include "common/exam.hpp"
#include "utils/assert.hpp"

#include <unordered_set>

namespace common {

Exam::Exam(
  std::string code,
  int credits,
  std::vector<std::string> students,
  std::vector<int> feasible_slots,
  std::vector<int> feasible_rooms
) {
  set_code(std::move(code));
  set_credits(credits);
  set_students(std::move(students));
  set_feasible_slots(std::move(feasible_slots));
  set_feasible_rooms(std::move(feasible_rooms));
}

void Exam::set_code(std::string code) {
  PANIC_IF(code.empty(), "Exam code cannot be an empty string");
  code_ = std::move(code);
}

void Exam::set_credits(int credits) {
  PANIC_IF(credits <= 0, "Exam '{}': Must have positive credits (got: {})", code_, credits);
  credits_ = credits;
}

void Exam::set_students(std::vector<std::string> students) {
#ifndef NDEBUG
  PANIC_IF(students.empty(), "Exam '{}': No registered students", code_);
  std::unordered_set<std::string_view> unique_checker;
  unique_checker.reserve(students.size());
  for (const std::string& s : students) {
    PANIC_IF(s.empty(), "Exam '{}': 'students' contains an empty ID", code_);
    PANIC_IF(
      !unique_checker.emplace(s).second,
      "Exam '{}': 'students' contains duplicate ID: '{}'", code_, s
    );
  }
#endif // NDEBUG
  students_ = std::move(students);
}

void Exam::set_feasible_slots(std::vector<int> feasible_slots) {
#ifndef NDEBUG
  PANIC_IF(feasible_slots.empty(), "Exam '{}': 'feasible_slots' is empty", code_);
  std::unordered_set<int> unique_checker;
  for (int s : feasible_slots) {
    PANIC_IF(s < 0, "Exam '{}': 'feasible_slots' has a negative value ({})", code_, s);
    PANIC_IF(
      !unique_checker.emplace(s).second,
      "Exam '{}': 'feasible_slots' has duplicate value ({})", code_, s
    );
  }
#endif // NDEBUG
  feasible_slots_ = std::move(feasible_slots);
}

void Exam::set_feasible_rooms(std::vector<int> feasible_rooms) {
#ifndef NDEBUG
  PANIC_IF(feasible_rooms.empty(), "Exam '{}': 'feasible_rooms' is empty", code_);
  std::unordered_set<int> unique_checker;
  for (int r : feasible_rooms) {
    PANIC_IF(r < 0, "Exam '{}': 'feasible_rooms' has a negative value ({})", code_, r);
    PANIC_IF(
      !unique_checker.emplace(r).second,
      "Exam '{}': 'feasible_rooms' has duplicate value ({})", code_, r
    );
  }
#endif // NDEBUG
  feasible_rooms_ = std::move(feasible_rooms);
}

} // namespace common