#pragma once

#include "exam.hpp"
#include "room.hpp"
#include "utils/matrix.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace common {

/**
 * @brief Stores the immutable, preprocessed data of an exam timetabling problem.
 * * ProblemData owns the static input data used by the solver, including exam
 * metadata, room metadata, conflict graphs, and initial feasibility constraints.
 * * The data is stored mostly in a Structure-of-Arrays (SoA) layout to improve
 * cache locality during repeated evaluation and construction of solutions.
 * * Dense matrices and CSR matrices are built once during initialization and are
 * treated as read-only afterward. This makes the object safe to share across
 * OpenMP threads, as long as no thread mutates it.
 */
struct ProblemData {

  // --- Problem dimensions ---
  const int num_exams;
  const int num_slots;
  const int num_rooms;
  const int num_sections;
  const int num_enrolments;
  
  // --- Student ID string <--> int mapping ---
  const std::unordered_map<std::string, int> student_to_id;
  const std::vector<std::string> id_to_student;

  // --- Section <--> Exam mapping ---
  const utils::CsrMatrix<int> exam_to_sections;
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
   * @brief The total weighted conflict degree for each exam.
   * * For each exam `i`, this stores the sum of weights (students) across all its 
   * neighbors in the conflict graph: `sum(conflicts_csrmatrix[i].values)`.
   * Used as a heuristic to identify "heavy" exams that are harder to schedule.
   */
  const std::vector<int> weighted_conflict_degrees;

  // --- Initial exam feasibility constraints ---

  // `feasible_slots[i]` tells the slots that are set to can be chosen for exam `i`
  const utils::CsrMatrix<int> feasible_slots;

  // `feasible_rooms[i]` tells the rooms that are set to can be chosen for exam `i`
  const utils::CsrMatrix<int> feasible_rooms;

  ProblemData(
    std::vector<Exam> _exams,
    std::vector<int64_t> _slot_timestamps,
    std::vector<Room> _rooms
  );  

private:
  static int extract_num_sections(const std::vector<Exam>& exams);
  static int extract_num_enrolments(const std::vector<Exam>& exams);
  static std::unordered_map<std::string, int> build_student_to_id(const std::vector<Exam>& exams, int num_enrolments);
  static std::vector<std::string> build_id_to_student(const std::unordered_map<std::string, int>& student_to_id);
  static std::vector<std::string> extract_exam_codes(const std::vector<Exam>& exams);
  static std::vector<int> extract_exam_credits(const std::vector<Exam>& exams);
  static std::vector<std::string> extract_section_codes(const std::vector<Exam>& exams, int num_sections);
  static utils::CsrMatrix<int> extract_section_students(
    const std::vector<Exam>& exams,
    const std::unordered_map<std::string, int>& student_to_id,
    int num_sections,
    int num_enrolments
  );
  static std::vector<int> extract_section_student_counts(const std::vector<Exam>& exams, int num_sections);
  static std::vector<int> extract_exam_student_counts(const std::vector<Exam>& exams);
  static utils::CsrMatrix<int> build_exam_to_sections(const std::vector<Exam>& exams);
  static std::vector<int> build_section_to_exam(const utils::CsrMatrix<int>& exam_to_sections);
  static std::vector<std::string> extract_room_codes(const std::vector<Room>& rooms);
  static std::vector<int> extract_room_capacities(const std::vector<Room>& rooms);
  static std::vector<std::string> extract_room_locations(const std::vector<Room>& rooms);
  static std::vector<std::string> extract_room_types(const std::vector<Room>& rooms);
  static utils::Matrix<int> build_conflicts_matrix(
    const std::vector<Exam>& exams,
    const std::unordered_map<std::string, int>& student_to_id
  );
  static std::vector<int> build_weighted_conflict_degrees(const utils::CsrMatrix<int>& conflicts_csrmatrix);
  static utils::CsrMatrix<int> build_feasible_slots(const std::vector<Exam>& exams, int num_slots);
  static utils::CsrMatrix<int> build_feasible_rooms(const std::vector<Exam>& exams, int num_rooms);

}; // struct ProblemData

} // namespace common