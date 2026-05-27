#pragma once

#include <vector>

#include "utils/matrix.hpp"

namespace common {

/**
 * @brief Represents an exam entity in the Examination Timetabling Problem.
 */
struct Exam {

  // --- Heuristic Weights & Metadata ---
  std::string code;  // Exam code (e.g. "INT1008 1")
  int credits = 1;
  int student_count = 0;

  // --- Core Operational Constraints ---
  std::vector<std::string> students;
  std::vector<int> feasible_slots;
  std::vector<int> feasible_rooms;
  std::vector<int> feasible_proctors;

  Exam(
    std::string _code,
    int _credits,
    std::vector<std::string> _students,
    std::vector<int> _feasible_slots,
    std::vector<int> _feasible_rooms,
    std::vector<int> _feasible_proctors
  );

}; // struct Exam

/**
 * @brief Manages the global collection of all exams and their interrelated constraints.
 * * This structure implements a highly optimized Structure of Arrays (SoA) layout 
 * designed for maximum cache locality and SIMD vectorization.
 * * All dense and sparse matrices (CSR format) representing conflict graphs and 
 * resource feasibility are computed once during initialization and frozen as constant 
 * data to ensure absolute thread safety in multi-threaded execution environments.
 */
struct Exams {

  const int num_exams;
  const int num_slots;
  const int num_rooms;
  const std::vector<std::string> codes;
  const std::vector<int> student_counts;
  const std::vector<int> credits;

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
  const std::vector<int> total_conflicts;

  const utils::CsrMatrix<int> feasible_slots;
  const utils::CsrMatrix<int> feasible_rooms;

  Exams(
    const std::vector<Exam>& _exams,
    int _num_slots,
    int _num_rooms
  );

private:
  static std::vector<std::string> extract_codes(const std::vector<Exam>& exams);
  static std::vector<int> extract_student_counts(const std::vector<Exam>& exams);
  static std::vector<int> extract_credits(const std::vector<Exam>& exams);
  static utils::Matrix<int> build_conflicts_matrix(const std::vector<Exam>& exams);
  static std::vector<int> build_total_conflicts(const utils::CsrMatrix<int>& conflicts_csrmatrix);
  static utils::CsrMatrix<int> build_feasible_slots(const std::vector<Exam>& exams, int num_slots);
  static utils::CsrMatrix<int> build_feasible_rooms(const std::vector<Exam>& exams, int num_rooms);

}; // struct Exams

} // namespace common