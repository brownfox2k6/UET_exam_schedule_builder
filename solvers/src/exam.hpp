#pragma once

#include <cstdint>
#include <string>

#include "utils/vector.hpp"

namespace cetp {

/**
 * Represents a section/class of a course exam.
 * Each section owns its student roster and room eligibility. All sections of the same Exam are
 * scheduled in the same time slot.
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
 * Represents an exam entity in the Examination Timetabling Problem.
 * An Exam may consist of multiple sections/classes scheduled in the same time slot. The Exam
 * defines feasible timestamps, while each section defines its own feasible rooms.
 */
struct Exam {
  // NOLINTBEGIN(*-non-private-*)

  // --- Heuristic Weights & Metadata ---
  std::string code;
  int credits = 0;
  int student_count = 0;

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
};  // struct Exam

}  // namespace cetp
