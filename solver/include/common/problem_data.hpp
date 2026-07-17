#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "exam.hpp"
#include "room.hpp"
#include "utils/matrix.hpp"

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
  const std::vector<std::string> id_to_student;
  const std::unordered_map<std::string_view, int> student_to_id;

  // --- Section <--> Exam mapping ---
  const std::vector<int>
      exam_to_sections;  // `exam_to_sections[i]` tells the first section index of exam `i`
  const std::vector<int> section_to_exam;

  // --- Exams metadata ---
  const std::vector<std::string> exam_codes;
  const std::vector<int> exam_credits;
  const std::vector<int> exam_student_counts;

  // --- Sections metadata ---
  const std::vector<std::string> section_codes;
  const utils::CsrMatrix<int> section_students;
  const std::vector<int> section_student_counts;

  // --- Time slots metadata ---
  const std::vector<int64_t> slot_timestamps;

  // --- Rooms metadata ---
  const std::vector<std::string> room_codes;
  const std::vector<int> room_capacities;
  const std::vector<std::string> room_locations;
  const std::vector<std::string> room_types;

  // --- Conflict graph ---

  // `conflicts_matrix(i, j)` tells the number of students taking both exams `i` and `j`
  const utils::Matrix<int> conflicts_matrix;

  /**
   * @brief CSR matrix representation of `conflicts_matrix`.
   * * Accessing row `i` returns a `CsrRowView<int>` providing two spans:
   * - `indices`: The IDs of exams `j` that share at least one student with exam `i`.
   * - `values`: The exact number of common students co-registered in both exam `i` and exam `j`.
   */
  const utils::CsrMatrix<int> conflicts_csrmatrix;

  /**
   * @brief Weighted conflict degree for each exam.
   * * `weighted_conflict_degrees[i]` = sum of shared students counts between exam `i` and each
   * conflicting exam `j` (i.e., sum of all entries in row `i` of the `conflicts_matrix`).
   * Used as ordering criterion (3) in `Solution::get_next_exam()`.
   */
  const std::vector<int> weighted_conflict_degrees;

  // --- Initial exam feasibility constraints ---

  // `feasible_slots[i]` tells the slots that are set to can be chosen for exam `i`
  const utils::CsrMatrix<int> feasible_slots;

  // `feasible_rooms[i]` tells the rooms that are set to can be chosen for exam `i`
  const utils::CsrMatrix<int> feasible_rooms;
  // NOLINTEND(*-non-private-member-*, *-const-*)

  ProblemData(const std::vector<Exam>& _exams, std::vector<int64_t> _slot_timestamps,
              const std::vector<Room>& _rooms);

 private:
  static auto extract_num_sections(const std::vector<Exam>& exams) -> int;
  static auto extract_num_enrollments(const std::vector<Exam>& exams) -> int;
  static auto build_id_to_student(const std::vector<Exam>& exams, int num_enrollments)
      -> std::vector<std::string>;
  static auto build_student_to_id(const std::vector<std::string>& id_to_student)
      -> std::unordered_map<std::string_view, int>;
  static auto extract_exam_codes(const std::vector<Exam>& exams) -> std::vector<std::string>;
  static auto extract_exam_credits(const std::vector<Exam>& exams) -> std::vector<int>;
  static auto extract_section_codes(const std::vector<Exam>& exams, int num_sections)
      -> std::vector<std::string>;
  static auto extract_section_students(
      const std::vector<Exam>& exams,
      const std::unordered_map<std::string_view, int>& student_to_id, int num_sections,
      int num_enrollments) -> utils::CsrMatrix<int>;
  static auto extract_section_student_counts(const std::vector<Exam>& exams, int num_sections)
      -> std::vector<int>;
  static auto extract_exam_student_counts(const std::vector<Exam>& exams) -> std::vector<int>;
  static auto build_exam_to_sections(const std::vector<Exam>& exams) -> std::vector<int>;
  static auto build_section_to_exam(const std::vector<Exam>& exams, int num_sections)
      -> std::vector<int>;
  static auto extract_room_codes(const std::vector<Room>& rooms) -> std::vector<std::string>;
  static auto extract_room_capacities(const std::vector<Room>& rooms) -> std::vector<int>;
  static auto extract_room_locations(const std::vector<Room>& rooms) -> std::vector<std::string>;
  static auto extract_room_types(const std::vector<Room>& rooms) -> std::vector<std::string>;
  static auto build_conflicts_matrix(const std::vector<Exam>& exams,
                                     const std::unordered_map<std::string_view, int>& student_to_id)
      -> utils::Matrix<int>;
  static auto build_weighted_conflict_degrees(const utils::CsrMatrix<int>& conflicts_csrmatrix)
      -> std::vector<int>;
  static auto build_feasible_slots(const std::vector<Exam>& exams, int num_slots)
      -> utils::CsrMatrix<int>;
  static auto build_feasible_rooms(const std::vector<Exam>& exams, int num_rooms)
      -> utils::CsrMatrix<int>;

};  // struct ProblemData

}  // namespace common