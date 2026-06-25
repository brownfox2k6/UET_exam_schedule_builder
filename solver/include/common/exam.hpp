#pragma once

#include <string>
#include <vector>

namespace common {

/**
 * @brief Represents a section/class of an exam.
 * * An ExamSection contains the students of a specific class/group belonging
 * to an Exam. Sections of the same Exam are scheduled in the same time slot.
 */
struct ExamSection {
  ExamSection(std::string code, std::vector<std::string> students);

  [[nodiscard]] auto code() const -> const std::string& { return code_; }
  [[nodiscard]] auto student_count() const -> int { return int(students_.size()); }
  [[nodiscard]] auto students() const -> const std::vector<std::string>& { return students_; }

  void set_code(std::string code);
  void set_students(std::vector<std::string> students);

 private:
  std::string code_;
  std::vector<std::string> students_;
};  // struct ExamSection

/**
 * @brief Represents an exam entity in the Examination Timetabling Problem.
 * * An Exam may consist of multiple sections/classes. All sections of the same
 * Exam must be scheduled in the same time slot. The Exam also defines the
 * feasible slots and feasible rooms that can be used by its sections.
 */
struct Exam {
  Exam(std::string code, int credits, std::vector<ExamSection> sections,
       std::vector<int> feasible_slots, std::vector<int> feasible_rooms);

  [[nodiscard]] auto code() const -> const std::string& { return code_; }
  [[nodiscard]] auto credits() const -> int { return credits_; }
  [[nodiscard]] auto sections() const -> const std::vector<ExamSection>& { return sections_; }
  [[nodiscard]] auto feasible_slots() const -> const std::vector<int>& { return feasible_slots_; }
  [[nodiscard]] auto feasible_rooms() const -> const std::vector<int>& { return feasible_rooms_; }
  [[nodiscard]] auto section_count() const -> int { return int(sections_.size()); }
  [[nodiscard]] auto student_count() const -> int { return student_count_; }

  void set_code(std::string code);
  void set_credits(int credits);
  void set_sections(std::vector<ExamSection> sections);
  void set_feasible_slots(std::vector<int> feasible_slots);
  void set_feasible_rooms(std::vector<int> feasible_rooms);

 private:
  // --- Heuristic Weights & Metadata ---
  std::string code_;
  int credits_ = 0;
  int student_count_ = 0;

  // --- Core Operational Constraints ---
  std::vector<ExamSection> sections_;
  std::vector<int> feasible_slots_;
  std::vector<int> feasible_rooms_;
};  // struct Exam

}  // namespace common