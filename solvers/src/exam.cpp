#include "exam.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "utils/vector.hpp"

namespace cetp {
namespace {

void validate_code(std::string_view code, std::string_view message) {
  if (code.empty()) {
    throw std::invalid_argument(std::format("{} must not be empty", message));
  }
}

void sort_and_validate_students(std::string_view section_code, Vector<std::string>& students) {
  if (students.empty()) {
    throw std::invalid_argument(
        std::format("Section '{}': students must not be empty", section_code)
    );
  }
  std::ranges::sort(students);
  if (students.front().empty()) {
    throw std::invalid_argument(
        std::format("Section '{}': students must have nonempty IDs", section_code)
    );
  }
  const auto duplicate = std::ranges::adjacent_find(students);
  if (duplicate != students.end()) {
    throw std::invalid_argument(
        std::format(
            "Section '{}': students must have unique IDs (duplicate: {})",
            section_code,
            *duplicate
        )
    );
  }
}

void sort_and_validate_feasible_rooms(
    std::string_view section_code,
    Vector<std::string>& feasible_rooms
) {
  if (feasible_rooms.empty()) {
    throw std::invalid_argument(
        std::format("Section '{}': feasible_rooms must not be empty", section_code)
    );
  }
  std::ranges::sort(feasible_rooms);
  if (feasible_rooms.front().empty()) {
    throw std::invalid_argument(
        std::format("Section '{}': feasible_rooms must have nonempty codes", section_code)
    );
  }
  const auto duplicate = std::ranges::adjacent_find(feasible_rooms);
  if (duplicate != feasible_rooms.end()) {
    throw std::invalid_argument(
        std::format(
            "Section '{}': feasible_rooms must have unique codes (duplicate: '{}')",
            section_code,
            *duplicate
        )
    );
  }
}

void validate_credits(std::string_view exam_code, int credits) {
  if (credits <= 0) {
    throw std::invalid_argument(
        std::format("Exam '{}': credits must be positive (got: {})", exam_code, credits)
    );
  }
}

auto sort_and_validate_sections(std::string_view exam_code, Vector<ExamSection>& sections) -> int {
  if (sections.empty()) {
    throw std::invalid_argument(std::format("Exam '{}': sections must not be empty", exam_code));
  }
  std::ranges::sort(sections, {}, &ExamSection::code);
  const auto duplicate_section = std::ranges::adjacent_find(sections, {}, &ExamSection::code);
  if (duplicate_section != sections.end()) {
    throw std::invalid_argument(
        std::format("Exam '{}': duplicate section code '{}'", exam_code, duplicate_section->code)
    );
  }

  int student_count = 0;
  for (const ExamSection& section : sections) {
    student_count += section.students.size();
  }

  struct StudentSection {
    std::string_view student;  // NOLINT(*-non-private-*)
    std::string_view section;  // NOLINT(*-non-private-*)
    auto operator<=>(const StudentSection&) const = default;
  };
  Vector<StudentSection> student_sections;
  student_sections.reserve(student_count);
  for (const ExamSection& section : sections) {
    for (std::string_view student : section.students) {
      student_sections.emplace_back(student, section.code);
    }
  }
  std::ranges::sort(student_sections);
  const auto duplicate_student =
      std::ranges::adjacent_find(student_sections, {}, &StudentSection::student);
  if (duplicate_student != student_sections.end()) {
    throw std::invalid_argument(
        std::format(
            "Exam '{}': student '{}' belongs to both sections '{}' and '{}'",
            exam_code,
            duplicate_student->student,
            duplicate_student->section,
            (duplicate_student + 1)->section
        )
    );
  }

  return student_count;
}

void sort_and_validate_feasible_timestamps(
    std::string_view exam_code,
    Vector<int64_t>& feasible_timestamps
) {
  if (feasible_timestamps.empty()) {
    throw std::invalid_argument(
        std::format("Exam '{}': feasible_timestamps must not be empty", exam_code)
    );
  }
  std::ranges::sort(feasible_timestamps);
  const auto duplicate = std::ranges::adjacent_find(feasible_timestamps);
  if (duplicate != feasible_timestamps.end()) {
    throw std::invalid_argument(
        std::format(
            "Exam '{}': feasible_timestamps must have unique values (duplicate: {})",
            exam_code,
            *duplicate
        )
    );
  }
}

}  // namespace

ExamSection::ExamSection(
    std::string _code,
    Vector<std::string> _students,
    Vector<std::string> _feasible_rooms
)
    : code(std::move(_code)),
      students(std::move(_students)),
      feasible_rooms(std::move(_feasible_rooms)) {
  validate_code(code, "Section code");
  sort_and_validate_students(code, students);
  sort_and_validate_feasible_rooms(code, feasible_rooms);
}

Exam::Exam(
    std::string _code,
    int _credits,
    Vector<ExamSection> _sections,
    Vector<int64_t> _feasible_timestamps
)
    : code(std::move(_code)),
      credits(_credits),
      sections(std::move(_sections)),
      feasible_timestamps(std::move(_feasible_timestamps)) {
  validate_code(code, "Exam code");
  validate_credits(code, this->credits);
  student_count = sort_and_validate_sections(code, sections);
  sort_and_validate_feasible_timestamps(code, feasible_timestamps);
}

}  // namespace cetp
