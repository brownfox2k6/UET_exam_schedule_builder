#pragma once

#include "common/exam.hpp"
#include <vector>

namespace common {

/**
 * @brief Represents a section/class of an exam.
 * * An ExamSection contains the students of a specific class/group belonging
 * to an Exam. Sections of the same Exam are scheduled in the same time slot.
 */
struct ExamSection {
  ExamSection(std::string code, std::vector<std::string> students);

  const std::string& code() const { return code_; }
  int student_count() const { return students_.size(); }
  const std::vector<std::string>& students() const { return students_; }

  void set_code(std::string code);
  void set_students(std::vector<std::string> students);

private:
  std::string code_;
  std::vector<std::string> students_;
}; // struct ExamSection

/**
 * @brief Represents an exam entity in the Examination Timetabling Problem.
 * * An Exam may consist of multiple sections/classes. All sections of the same
 * Exam must be scheduled in the same time slot. The Exam also defines the
 * feasible slots and feasible rooms that can be used by its sections.
 */
struct Exam {
  Exam(
    std::string code,
    int credits,
    std::vector<ExamSection> sections,
    std::vector<int> feasible_slots,
    std::vector<int> feasible_rooms
  );

  const std::string& code() const { return code_; }
  int credits() const { return credits_; }
  const std::vector<ExamSection>& sections() const { return sections_; }
  const std::vector<int>& feasible_slots() const { return feasible_slots_; }
  const std::vector<int>& feasible_rooms() const { return feasible_rooms_; }
  int section_count() const { return sections_.size(); }
  int student_count() const { return student_count_; }

  void set_code(std::string code);
  void set_credits(int credits);
  void set_sections(std::vector<ExamSection> sections);
  void set_feasible_slots(std::vector<int> feasible_slots);
  void set_feasible_rooms(std::vector<int> feasible_rooms);

private:
  // --- Heuristic Weights & Metadata ---
  std::string code_;
  int credits_;
  int student_count_;

  // --- Core Operational Constraints ---
  std::vector<ExamSection> sections_;
  std::vector<int> feasible_slots_;
  std::vector<int> feasible_rooms_;
}; // struct Exam

} // namespace common