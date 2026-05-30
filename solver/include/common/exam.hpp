#pragma once

#include <vector>

namespace common {

/**
 * @brief Represents an exam entity in the Examination Timetabling Problem.
 */
struct Exam {
  Exam() = default;

  Exam(
    std::string _code,
    int _credits,
    std::vector<std::string> _students,
    std::vector<int> _feasible_slots,
    std::vector<int> _feasible_rooms
  );

  std::string code() const { return code_; }
  int credits() const { return credits_; }
  int student_count() const { return students_.size(); }
  const std::vector<std::string>& students() const { return students_; }
  const std::vector<int>& feasible_slots() const { return feasible_slots_; }
  const std::vector<int>& feasible_rooms() const { return feasible_rooms_; }

  void set_code(std::string code);
  void set_credits(int credits);
  void set_students(std::vector<std::string> students);
  void set_feasible_slots(std::vector<int> feasible_slots);
  void set_feasible_rooms(std::vector<int> feasible_rooms);

private:
  // --- Heuristic Weights & Metadata ---
  std::string code_;  // Exam code (e.g. "INT1008 1")
  int credits_;

  // --- Core Operational Constraints ---
  std::vector<std::string> students_;
  std::vector<int> feasible_slots_;
  std::vector<int> feasible_rooms_;
}; // struct Exam

} // namespace common