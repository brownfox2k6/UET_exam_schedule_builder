#pragma once

#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>

#include "exam.hpp"
#include "room.hpp"
#include "utils/csr_matrix.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace cetp {

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
  // NOLINTBEGIN(*-non-private-member-*)
  // --- Problem dimensions ---
  int num_exams = 0;
  int num_slots = 0;
  int num_rooms = 0;
  int num_sections = 0;
  int num_students = 0;
  int num_enrollments = 0;

  // --- Student ID string <-> int mapping ---
  Vector<std::string> student_codes;

  // --- Section <-> Exam mapping ---

  // `exam_section_offsets[i]` tells the first section index of exam `i`.
  // It has size `num_exams + 1` with `exam_section_offsets[num_exams] = num_sections`
  Vector<int> exam_section_offsets;
  Vector<int> section_to_exam;

  // --- Exams metadata ---
  Vector<std::string> exam_codes;
  Vector<int> exam_credits;
  Vector<int> exam_student_counts;

  // --- Sections metadata ---
  Vector<std::string> section_codes;
  CsrMatrix<int> section_students;
  Vector<int> section_student_counts;

  // --- Time slots metadata ---
  Vector<int64_t> slot_timestamps;

  // --- Rooms metadata ---
  Vector<std::string> room_codes;
  Vector<int> room_capacities;
  Vector<std::string> room_locations;
  Vector<std::string> room_types;

  // --- Conflict graph ---

  // `conflicts_matrix(i, j)` tells the number of students taking both exams `i` and `j`
  Matrix<int> conflicts_matrix;

  /**
   * @brief CSR matrix representation of `conflicts_matrix`.
   * * Accessing row `i` returns a `CsrRowView<int>` providing two spans:
   * - `indices`: The IDs of exams `j` that share at least one student with exam `i`.
   * - `values`: The exact number of common students co-registered in both exam `i` and exam `j`.
   */
  CsrMatrix<int> conflicts_csr;

  /**
   * @brief Weighted conflict degree for each exam.
   * * `weighted_conflict_degrees[i]` = sum of shared students counts between exam `i` and each
   * conflicting exam `j` (i.e., sum of all entries in row `i` of the `conflicts_matrix`).
   * Used to prioritize unscheduled exams.
   */
  Vector<int> weighted_conflict_degrees;

  // --- Initial exam feasibility constraints ---

  // `feasible_slots_by_exam[i]` contains slots IDs eligible for exam `i`
  CsrMatrix<int> feasible_slots_by_exam;

  // `feasible_rooms_by_section[i]` contains room IDs eligible for section `i`.
  CsrMatrix<int> feasible_rooms_by_section;

  // NOLINTEND(*-non-private-member-*)

  ProblemData(const Vector<Exam>& exams, const Vector<Room>& rooms);

  [[nodiscard]] auto exams() const noexcept -> std::ranges::iota_view<int, int> {
    return std::views::iota(0, num_exams);
  }
  [[nodiscard]] auto sections() const noexcept -> std::ranges::iota_view<int, int> {
    return std::views::iota(0, num_sections);
  }
  [[nodiscard]] auto students() const noexcept -> std::ranges::iota_view<int, int> {
    return std::views::iota(0, num_students);
  }
  [[nodiscard]] auto slots() const noexcept -> std::ranges::iota_view<int, int> {
    return std::views::iota(0, num_slots);
  }
  [[nodiscard]] auto rooms() const noexcept -> std::ranges::iota_view<int, int> {
    return std::views::iota(0, num_rooms);
  }

  // Returns section indices belonging to the given `exam`
  [[nodiscard]] auto sections_of_exam(int exam) const noexcept -> std::ranges::iota_view<int, int>;

  // Binary search on sorted `student_codes` in O(log(num_students)) time.
  // Returns -1 if not found.
  [[nodiscard]] auto find_student_id(std::string_view student) const noexcept -> int;
};  // struct ProblemData

}  // namespace cetp
