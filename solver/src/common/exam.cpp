#include "common/exam.hpp"

#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "utils/assert.hpp"

namespace common {

// ---------------------------------
// ---------- ExamSection ----------
// ---------------------------------

ExamSection::ExamSection(std::string code, std::vector<std::string> students) {
  set_code(std::move(code));
  set_students(std::move(students));
}

void ExamSection::set_code(std::string code) {
  PANIC_IF(code.empty(), "Section code cannot be an empty string");
  code_ = std::move(code);
}

void ExamSection::set_students(std::vector<std::string> students) {
#ifndef NDEBUG
  PANIC_IF(students.empty(), "Section '{}': No registered students", code_);
  std::unordered_set<std::string_view> unique_checker;
  unique_checker.reserve(students.size());
  for (const std::string& student : students) {
    PANIC_IF(student.empty(), "Section '{}': 'students' contains an empty ID", code_);
    PANIC_IF(!unique_checker.emplace(student).second,
             "Section '{}': 'students' contains duplicate ID: '{}'", code_, student);
  }
#endif  // NDEBUG
  students_ = std::move(students);
}

// --------------------------
// ---------- Exam ----------
// --------------------------

Exam::Exam(std::string code, int credits, std::vector<ExamSection> sections,
           std::vector<int> feasible_slots, std::vector<int> feasible_rooms) {
  set_code(std::move(code));
  set_credits(credits);
  set_sections(std::move(sections));
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

void Exam::set_sections(std::vector<ExamSection> sections) {
  student_count_ = 0;
  for (const ExamSection& section : sections) {
    student_count_ += section.student_count();
  }
#ifndef NDEBUG
  PANIC_IF(sections.empty(), "Exam '{}': 'sections' is empty", code_);
  std::unordered_set<std::string_view> unique_section_checker;
  unique_section_checker.reserve(sections.size());
  std::unordered_map<std::string_view, std::string_view> unique_student_checker;
  unique_student_checker.reserve(size_t(student_count_));
  for (const ExamSection& section : sections) {
    const std::string_view section_code = section.code();
    PANIC_IF(!unique_section_checker.emplace(section_code).second,
             "Exam '{}' contains duplicate section ID: '{}'", code_, section.code());
    for (const std::string_view student : section.students()) {
      auto [it, inserted] = unique_student_checker.emplace(student, section_code);  // NOLINT
      PANIC_IF(!inserted, "Exam '{}': Student '{}' belongs to more than one section: '{}' and '{}'",
               code_, student, it->second, section_code);
    }
  }
#endif  // NDEBUG
  sections_ = std::move(sections);
}

void Exam::set_feasible_slots(std::vector<int> feasible_slots) {
#ifndef NDEBUG
  PANIC_IF(feasible_slots.empty(), "Exam '{}': 'feasible_slots' is empty", code_);
  std::unordered_set<int> unique_checker;
  for (int slot : feasible_slots) {
    PANIC_IF(slot < 0, "Exam '{}': 'feasible_slots' has a negative value ({})", code_, slot);
    PANIC_IF(!unique_checker.emplace(slot).second,
             "Exam '{}': 'feasible_slots' has duplicate value ({})", code_, slot);
  }
#endif  // NDEBUG
  feasible_slots_ = std::move(feasible_slots);
}

void Exam::set_feasible_rooms(std::vector<int> feasible_rooms) {
#ifndef NDEBUG
  PANIC_IF(feasible_rooms.empty(), "Exam '{}': 'feasible_rooms' is empty", code_);
  std::unordered_set<int> unique_checker;
  for (int room : feasible_rooms) {
    PANIC_IF(room < 0, "Exam '{}': 'feasible_rooms' has a negative value ({})", code_, room);
    PANIC_IF(!unique_checker.emplace(room).second,
             "Exam '{}': 'feasible_rooms' has duplicate value ({})", code_, room);
  }
#endif  // NDEBUG
  feasible_rooms_ = std::move(feasible_rooms);
}

}  // namespace common