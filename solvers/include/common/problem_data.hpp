#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "exam.hpp"
#include "room.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace common {

/**
 * @brief Stores the immutable, preprocessed data of an exam timetabling problem.
 * * ProblemData owns the static input data used by the solver, including exam metadata, room
 * metadata, conflict graphs, and initial feasibility constraints.
 * * The data is stored mostly in a Structure-of-Arrays (SoA) layout to improve cache locality
 * during repeated evaluation and construction of solutions.
 * * Dense matrices and CSR matrices are built once during initialization and are treated as
 * read-only afterward. This makes the object safe to share across OpenMP threads, as long as no
 * thread mutates it.
 */
struct ProblemData {
  // NOLINTBEGIN(*-non-private-member-*, *-const-*)
  // --- Problem dimensions ---
  const int num_exams;
  const int num_slots;
  const int num_rooms;
  const int num_sections;
  const int num_enrollments;

  // --- Student ID string <--> int mapping ---
  const Vector<std::string> id_to_student;
  const std::unordered_map<std::string_view, int> student_to_id;

  // --- Section <--> Exam mapping ---
  const Vector<int>
      exam_to_sections;  // `exam_to_sections[i]` tells the first section index of exam `i`
  const Vector<int> section_to_exam;

  // --- Exams metadata ---
  const Vector<std::string> exam_codes;
  const Vector<int> exam_credits;
  const Vector<int> exam_student_counts;

  // --- Sections metadata ---
  const Vector<std::string> section_codes;
  const CsrMatrix<int> section_students;
  const Vector<int> section_student_counts;

  // --- Time slots metadata ---
  const Vector<int64_t> slot_timestamps;

  // --- Rooms metadata ---
  const Vector<std::string> room_codes;
  const Vector<int> room_capacities;
  const Vector<std::string> room_locations;
  const Vector<std::string> room_types;

  // --- Conflict graph ---

  // `conflicts_matrix(i, j)` tells the number of students taking both exams `i` and `j`
  const Matrix<int> conflicts_matrix;

  /**
   * @brief CSR matrix representation of `conflicts_matrix`.
   * * Accessing row `i` returns a `CsrRowView<int>` providing two spans:
   * - `indices`: The IDs of exams `j` that share at least one student with exam `i`.
   * - `values`: The exact number of common students co-registered in both exam `i` and exam `j`.
   */
  const CsrMatrix<int> conflicts_csrmatrix;

  /**
   * @brief Weighted conflict degree for each exam.
   * * `weighted_conflict_degrees[i]` = sum of shared students counts between exam `i` and each
   * conflicting exam `j` (i.e., sum of all entries in row `i` of the `conflicts_matrix`).
   * Used as ordering criterion (3) in `Solution::get_next_exam()`.
   */
  const Vector<int> weighted_conflict_degrees;

  // --- Initial exam feasibility constraints ---

  // `feasible_slots_by_exam[i]` tells the slots that are set to can be chosen for exam `i`
  const CsrMatrix<int> feasible_slots_by_exam;

  // `feasible_rooms_by_section[i]` contains room IDs eligible for section `i`.
  const CsrMatrix<int> feasible_rooms_by_section;
  // NOLINTEND(*-non-private-member-*, *-const-*)

  ProblemData(const Vector<Exam>& _exams, Vector<int64_t> _slot_timestamps,
              const Vector<Room>& _rooms);

  ~ProblemData() = default;
  ProblemData(const ProblemData&) = delete;
  auto operator=(const ProblemData&) -> ProblemData& = delete;
  ProblemData(ProblemData&&) = delete;
  auto operator=(ProblemData&&) -> ProblemData& = delete;

  // returns [first, last)
  [[nodiscard]] auto sections_of_exam(int exam) const -> std::pair<int, int>;

};  // struct ProblemData

}  // namespace common
