#pragma once

#include <string>

#include "utils/vector.hpp"

namespace common {

/**
 * @brief Represents a section/class of a course exam.
 * * Each section owns its student roster and room eligibility. All sections of
 * the same Exam are scheduled in the same time slot.
 */
struct ExamSection {
  ExamSection(std::string code, Vector<std::string> students, Vector<int> feasible_rooms);

  [[nodiscard]] auto code() const -> const std::string& { return code_; }
  [[nodiscard]] auto student_count() const -> int { return students_.size(); }
  [[nodiscard]] auto students() const -> const Vector<std::string>& { return students_; }
  [[nodiscard]] auto feasible_rooms() const -> const Vector<int>& { return feasible_rooms_; }

  void set_code(std::string code);
  void set_students(Vector<std::string> students);
  void set_feasible_rooms(Vector<int> feasible_rooms);

 private:
  std::string code_;
  Vector<std::string> students_;
  Vector<int> feasible_rooms_;
};  // struct ExamSection

/**
 * @brief Represents an exam entity in the Examination Timetabling Problem.
 * * An Exam may consist of multiple sections/classes. All sections of the same
 * Exam must be scheduled in the same time slot. The Exam defines feasible
 * time slots, while each section defines its own feasible rooms.
 */
struct Exam {
  Exam(std::string code, int credits, Vector<ExamSection> sections, Vector<int> feasible_slots);

  [[nodiscard]] auto code() const -> const std::string& { return code_; }
  [[nodiscard]] auto credits() const -> int { return credits_; }
  [[nodiscard]] auto sections() const -> const Vector<ExamSection>& { return sections_; }
  [[nodiscard]] auto feasible_slots() const -> const Vector<int>& { return feasible_slots_; }
  [[nodiscard]] auto section_count() const -> int { return sections_.size(); }
  [[nodiscard]] auto student_count() const -> int { return student_count_; }

  void set_code(std::string code);
  void set_credits(int credits);
  void set_sections(Vector<ExamSection> sections);
  void set_feasible_slots(Vector<int> feasible_slots);

 private:
  // --- Heuristic Weights & Metadata ---
  std::string code_;
  int credits_ = 0;
  int student_count_ = 0;

  // --- Core Operational Constraints ---
  Vector<ExamSection> sections_;
  Vector<int> feasible_slots_;
};  // struct Exam

}  // namespace common
