#pragma once

#include <cstdint>
#include <string>

#include "utils/vector.hpp"

namespace cetp {

/**
 * @brief Represents a section/class of a course exam.
 * * Each section owns its student roster and room eligibility. All sections of
 * the same Exam are scheduled in the same time slot.
 */
struct ExamSection {
  // NOLINTBEGIN(*-non-private-*)
  std::string code;
  Vector<std::string> students;
  Vector<std::string> feasible_rooms;
  // NOLINTEND(*-non-private-*)

  ExamSection(std::string code, Vector<std::string> students, Vector<std::string> feasible_rooms);
};  // struct ExamSection

/**
 * @brief Represents an exam entity in the Examination Timetabling Problem.
 * * An Exam may consist of multiple sections/classes. All sections of the same
 * Exam must be scheduled in the same time slot. The Exam defines feasible
 * timestamps, while each section defines its own feasible rooms.
 */
struct Exam {
  // NOLINTBEGIN(*-non-private-*)

  // --- Heuristic Weights & Metadata ---
  std::string code;
  int credits;
  int student_count;

  // --- Core Operational Constraints ---
  Vector<ExamSection> sections;
  Vector<int64_t> feasible_timestamps;

  // NOLINTEND(*-non-private-*)

  Exam(
      std::string code,
      int credits,
      Vector<ExamSection> sections,
      Vector<int64_t> feasible_timestamps
  );
};  // class Exam

}  // namespace cetp
