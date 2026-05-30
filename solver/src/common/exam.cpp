#include "common/exam.hpp"
#include "utils/assert.hpp"

#include <algorithm>
#include <unordered_set>

namespace common {

Exam::Exam(
  std::string _code,
  int _credits,
  std::vector<std::string> _students,
  std::vector<int> _feasible_slots,
  std::vector<int> _feasible_rooms
):
  code(std::move(_code)),
  credits(_credits),
  students(std::move(_students)),
  feasible_slots(std::move(_feasible_slots)),
  feasible_rooms(std::move(_feasible_rooms))
{
#ifndef NDEBUG
  PANIC_IF(code.empty(), "Exam code cannot be an empty string");
  PANIC_IF(credits <= 0, "Exam '{}': Must have positive credits (got: {})", code, credits);
  PANIC_IF(students.empty(), "Exam '{}': No registered students", code);

  std::unordered_set<std::string_view> unique_students_checker;
  unique_students_checker.reserve(students.size());
  for (const std::string& s : students) {
    PANIC_IF(s.empty(), "Exam '{}': 'students' contains an empty ID", code);
    PANIC_IF(
      !unique_students_checker.emplace(s).second,
      "Exam '{}': 'students' contains duplicate ID: '{}'", code, s
    );
  }
  std::unordered_set<int> unique_checker;
  unique_checker.reserve(std::max({
    feasible_slots.size(), feasible_rooms.size()
  }));
  auto validate_feasible_sets = [&](
    const std::vector<int>& values,
    std::string_view field_name
  ) {
    PANIC_IF(values.empty(), "Exam '{}': {} is empty", code, field_name);
    unique_checker.clear();
    for (int value : values) {
      PANIC_IF(value < 0, "Exam '{}': '{}' has a negative value ({})", code, field_name, value);
      PANIC_IF(
        !unique_checker.emplace(value).second,
        "Exam '{}': '{}' has duplicate value ({})", code, field_name, value
      );
    }
  };
  validate_feasible_sets(feasible_slots, "feasible_slots");
  validate_feasible_sets(feasible_rooms, "feasible_rooms");
#endif // NDEBUG
}

} // namespace common