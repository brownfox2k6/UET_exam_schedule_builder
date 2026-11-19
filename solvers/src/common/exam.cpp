#include "common/exam.hpp"

#include <format>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace common {

// ------------------------------------------------------------------------------------------------
// ExamSection  -----------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------

ExamSection::ExamSection(std::string code, Vector<std::string> students,
                         Vector<int> feasible_rooms) {
  set_code(std::move(code));
  set_students(std::move(students));
  set_feasible_rooms(std::move(feasible_rooms));
}

void ExamSection::set_code(std::string code) {
  if (code.empty()) {
    throw std::invalid_argument("Section code must not be empty");
  }
  code_ = std::move(code);
}

void ExamSection::set_students(Vector<std::string> students) {
  if (students.empty()) {
    throw std::invalid_argument(std::format("Section '{}': students must not be empty", code_));
  }
  std::unordered_set<std::string_view> unique_checker;
  unique_checker.reserve(size_t(students.size()));
  for (const std::string& student : students) {
    if (student.empty()) {
      throw std::invalid_argument(
          std::format("Section '{}': students must have nonempty IDs", code_));
    }
    const auto [position, inserted] = unique_checker.emplace(student);
    if (!inserted) {
      throw std::invalid_argument(std::format(
          "Section '{}': students must have unique IDs (duplicate: {})", code_, student));
    }
  }
  students_ = std::move(students);
}

void ExamSection::set_feasible_rooms(Vector<int> feasible_rooms) {
  if (feasible_rooms.empty()) {
    throw std::invalid_argument(
        std::format("Section '{}': feasible_rooms must not be empty", code_));
  }
  std::unordered_set<int> unique_checker;
  unique_checker.reserve(size_t(feasible_rooms.size()));
  for (int room : feasible_rooms) {
    if (room < 0) {
      throw std::invalid_argument(std::format(
          "Section '{}': feasible_rooms must have nonnegative IDs (got: {})", code_, room));
    }
    const auto [position, inserted] = unique_checker.emplace(room);
    if (!inserted) {
      throw std::invalid_argument(std::format(
          "Section '{}': feasible_rooms must have unique IDs (duplicate: {})", code_, room));
    }
  }
  feasible_rooms_ = std::move(feasible_rooms);
}

// ------------------------------------------------------------------------------------------------
// Exam  ------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------

Exam::Exam(std::string code, int credits, Vector<ExamSection> sections,
           Vector<int> feasible_slots) {
  set_code(std::move(code));
  set_credits(credits);
  set_sections(std::move(sections));
  set_feasible_slots(std::move(feasible_slots));
}

void Exam::set_code(std::string code) {
  if (code.empty()) {
    throw std::invalid_argument("Exam code must not be empty");
  }
  code_ = std::move(code);
}

void Exam::set_credits(int credits) {
  if (credits <= 0) {
    throw std::invalid_argument(
        std::format("Exam '{}': credits must be positive (got: {})", code_, credits));
  }
  credits_ = credits;
}

void Exam::set_sections(Vector<ExamSection> sections) {
  if (sections.empty()) {
    throw std::invalid_argument(std::format("Exam '{}': sections must not be empty", code_));
  }
  student_count_ = 0;
  for (const ExamSection& section : sections) {
    student_count_ += section.student_count();
  }
  std::unordered_set<std::string_view> unique_section_checker;
  unique_section_checker.reserve(size_t(sections.size()));
  std::unordered_map<std::string_view, std::string_view> unique_student_checker;
  unique_student_checker.reserve(size_t(student_count_));
  for (const ExamSection& section : sections) {
    const std::string_view section_code = section.code();
    if (!unique_section_checker.emplace(section_code).second) {
      throw std::invalid_argument(
          std::format("Exam '{}': duplicate section code '{}'", code_, section_code));
    }
    for (const std::string_view student : section.students()) {
      const auto [position, inserted] = unique_student_checker.emplace(student, section_code);
      if (!inserted) {
        throw std::invalid_argument(
            std::format("Exam '{}': student '{}' belongs to both sections '{}' and '{}'", code_,
                        student, position->second, section_code));
      }
    }
  }
  sections_ = std::move(sections);
}

void Exam::set_feasible_slots(Vector<int> feasible_slots) {
  if (feasible_slots.empty()) {
    throw std::invalid_argument(std::format("Exam '{}': feasible_slots must not be empty", code_));
  }
  std::unordered_set<int> unique_checker;
  unique_checker.reserve(size_t(feasible_slots.size()));
  for (int slot : feasible_slots) {
    if (slot < 0) {
      throw std::invalid_argument(std::format(
          "Exam '{}': feasible_slots must have nonnegative IDs (got: {})", code_, slot));
    }
    const auto [position, inserted] = unique_checker.emplace(slot);
    if (slot < 0 || !inserted) {
      throw std::invalid_argument(std::format(
          "Exam '{}': feasible_slots must have unique IDs (duplicate: {})", code_, slot));
    }
  }
  feasible_slots_ = std::move(feasible_slots);
}

}  // namespace common
