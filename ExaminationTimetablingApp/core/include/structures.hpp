#pragma once

#include <cstddef>
#include <vector>
#include <cassert>
#include <cmath>

namespace common {

/**
* @brief A generic 2D matrix structure.
* Implements a flat 1D vector back-end to ensure memory continuity. 
*/
template<typename T>
struct Matrix {
private:
  std::vector<T> data;

public:
  const size_t rows;
  const size_t cols;

  Matrix(size_t r, size_t c, T value = T())
    : rows(r), cols(c)
  {
    assert(r > 0 && "Matrix rows must be positive.");
    assert(c > 0 && "Matrix columns must be positive.");
    data.assign(rows * cols, value);
  }

  Matrix(const std::vector<std::vector<T>>& list)
    : Matrix(list.size(), list[0].size())
  {
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        data(i, j) = list[i][j];
      }
    }
  }

  inline T& operator()(size_t r, size_t c) {
    assert(r < rows && "Row index out-of-bounds.");
    assert(c < cols && "Column index out-of-bounds.");
    return data[r * cols + c];
  }

  inline T operator()(size_t r, size_t c) const {
    assert(r < rows && "Row index out-of-bounds.");
    assert(c < cols && "Column index out-of-bounds.");
    return data[r * cols + c];
  }

  void fill(T value) {
    std::fill(data.begin(), data.end(), value);
  }
};

/**
 * @brief A generic Compressed Sparse Row (CSR) matrix structure.
 * Implements flat 1D vectors for data and offsets to ensure memory continuity and fast, cache-friendly row iteration.
 */
template<typename T>
struct CsrMatrix {
private:
  std::vector<T> columns;
  std::vector<size_t> offsets;

public:
  CsrMatrix() = default;

  CsrMatrix(const common::Matrix<T>& matrix) {
    offsets.assign(matrix.rows + 1, 0);
    for (size_t i = 0; i < matrix.rows; ++i) {
      for (size_t j = 0; j < matrix.cols; ++j) {
        if (i != j && matrix(i, j) > 0) {
          columns.emplace_back(static_cast<T>(j));
        }
      }
      offsets[i + 1] = columns.size();
    }
    columns.shrink_to_fit();
  }

  auto row_range(size_t row_index) const {
    return std::make_pair(
      columns.begin() + offsets[row_index],
      columns.begin() + offsets[row_index + 1]
    );
  }
};

} // namespace common


namespace aco {

/**
* @brief Represents an individual ant constructing an exam timetable.
* The schedule acts as a direct mapping: the vector index represents the Exam ID, 
* and the stored value represents the assigned Timeslot ID (-1 for unassigned).
* The fitness tracks soft constraint violations (lower is better).
*/
struct Ant {
private:
  const size_t num_exams;
  const size_t num_slots;

public:
  // `conflicting_exams_count(i, j)` tells how many exams `k` that 
  // have conflict with exam `i` and have been assigned to slot `j`.
  // If this value is > 0, slot `j` is strictly forbidden for exam `i`.
  common::Matrix<int> conflicting_exams_count;

  // `forbidden_slots_count[i]` tells how many slots that exam `i` cannot be assigned to. 
  // That is, `forbidden_slots_count[i] = count(conflicting_exams_count(i, j) > 0)`
  std::vector<int> forbidden_slots_count;

  // `feasible_slots_count[i]` tells how many slots that exam `i` can be assigned to. 
  // That is, `feasible_slots_count[i] = count(conflicting_exams_count(i, j) == 0)`
  std::vector<int> feasible_slots_count;

  std::vector<int> schedule;
  common::Matrix<int> feasible_slots;
  double fitness;

  Ant(size_t n_exams, size_t n_slots)
    : num_exams(n_exams),
      num_slots(n_slots),
      schedule(num_exams, -1),
      feasible_slots(num_exams, num_slots),
      feasible_slots_count(num_exams, num_slots),
      fitness(0.0),
      conflicting_exams_count(num_exams, num_slots, 0),
      forbidden_slots_count(num_exams, 0) 
  {
    reset_feasible_slots();
  }

  Ant& operator=(const Ant& other) {
    if (this != &other) {
      this->schedule = other.schedule;
      this->fitness = other.fitness;
    }
    return *this;
  }

  void reset_feasible_slots() {
    for (size_t exam = 0; exam < num_exams; ++exam) {
      for (size_t slot = 0; slot < num_slots; ++slot) {
        feasible_slots(exam, slot) = slot;
      }
    }
  }

  void reset() {
    std::fill(schedule.begin(), schedule.end(), -1);
    reset_feasible_slots();
    std::fill(feasible_slots_count.begin(), feasible_slots_count.end(), num_slots);
    fitness = 0.0;
    conflicting_exams_count.fill(0);
    std::fill(forbidden_slots_count.begin(), forbidden_slots_count.end(), 0);
  }

  int get_next_exam(const std::vector<int>& total_student_conflict) const {
    int best_exam = -1;
    int max_degree = -1;
    int max_conflict = -1;
    for (size_t exam = 0; exam < num_exams; ++exam) {
      if (schedule[exam] != -1) {
        continue;
      }
      int degree = forbidden_slots_count[exam];
      int conflict = total_student_conflict[exam];
      if (std::tie(degree, conflict) > std::tie(max_degree, max_conflict)) {
        max_degree = degree;
        max_conflict = conflict;
        best_exam = exam;
      }
    }
    return best_exam;
  }

  void assign_exam(
    int exam,
    int slot,
    const common::CsrMatrix<int>& conflict_exams,
    double penalty = 0.0
  ) {
    schedule[exam] = slot;
    fitness += penalty;
    auto [first, last] = conflict_exams.row_range(exam);
    for (auto it = first; it != last; ++it) {
      int neighbor_exam = *it;
      if (conflicting_exams_count(neighbor_exam, slot) == 0) {
        ++forbidden_slots_count[neighbor_exam];
        int target = -1;
        for (size_t j = 0; j < feasible_slots_count[neighbor_exam]; ++j) {
          if (feasible_slots(neighbor_exam, j) == slot) {
            target = j;
            break;
          }
        }
        if (target != -1) {
          feasible_slots(neighbor_exam, target)
            = feasible_slots(neighbor_exam, --feasible_slots_count[neighbor_exam]);
          ;
        }
      }
      ++conflicting_exams_count(neighbor_exam, slot);
    }
  }

  void unassign_exam(
    int exam,
    const common::CsrMatrix<int>& conflict_exams
  ) {
    int old_slot = schedule[exam];
    schedule[exam] = -1;
    auto [first, last] = conflict_exams.row_range(exam);
    for (auto it = first; it != last; ++it) {
      int neighbor_exam = *it;
      if (--conflicting_exams_count(neighbor_exam, old_slot) == 0) {
        --forbidden_slots_count[neighbor_exam];
        feasible_slots(neighbor_exam, feasible_slots_count[neighbor_exam]++) = old_slot;
      }
    }
  }

  double calculate_delta_penalty(
    int exam,
    int new_slot,
    const common::Matrix<int>& student_conflict,
    const common::CsrMatrix<int>& conflict_exams,
    const std::vector<double>& absolute_day_slots,
    const int ignore_exam = -1
  ) const {
    double delta = 0.0;
    double d_old = schedule[exam] != -1 ? absolute_day_slots[schedule[exam]] : -1.0;
    double d_new = absolute_day_slots[new_slot];
    auto [first, last] = conflict_exams.row_range(exam);
    for (auto it = first; it != last; ++it) {
      int e = *it;
      int weight = student_conflict(exam, e);
      if (weight == 0 || e == ignore_exam || e == exam || schedule[e] == -1) {
        continue;
      }
      double d_cur = absolute_day_slots[schedule[e]];
      double penalty_old = schedule[exam] != -1 ? weight * std::exp2(-std::fabs(d_old - d_cur)) : 0.0;
      double penalty_new = weight * std::exp2(-std::fabs(d_new - d_cur));
      delta += penalty_new - penalty_old;
    }
    return delta;
  }
};

} // namespace aco