#include "common/evaluator.hpp"
#include "utils/assert.hpp"
#include "utils/matrix.hpp"

#include <iterator>
#include <numeric>
#include <string>
#include <unordered_set>
#include <vector>

constexpr double SECONDS_PER_DAY = 86400.0;
constexpr double HARD_CONSTRAINT_PENALTY = 1e9;

namespace common {

Evaluator::Evaluator(
  const Hyperparams& _hyperparams,
  std::vector<Exam> _exams,
  const std::vector<int64_t>& _slot_timestamps
):
  hyperparams(_hyperparams.eval),
  num_exams(_exams.size()),
  num_slots(_slot_timestamps.size()),
  proximity_penalties([&]() {
    utils::Matrix<double> penalties(num_slots, num_slots, 0.0);
    for (int i = 0; i < num_slots; ++i) {
      penalties(i, i) = HARD_CONSTRAINT_PENALTY;
      const int64_t slot_i = _slot_timestamps[i];
      for (int j = i + 1; j < num_slots; ++j) {
        const double diff_days = std::abs(slot_i - _slot_timestamps[j]) / SECONDS_PER_DAY;
        const double value = std::pow(hyperparams.penalty_decay_base, -diff_days);
        penalties(i, j) = penalties(j, i) = value;
      }
    }
    return penalties;
  }()),
  student_conflicts_matrix([&]() {
    utils::Matrix<int> conflict(num_exams, num_exams, 0);
    for (int i = 0; i < num_exams; ++i) {
      std::unordered_set<std::string> students_i;
      students_i.reserve(_exams[i].student_count);
      students_i.insert(
        std::make_move_iterator(_exams[i].students.begin()),
        std::make_move_iterator(_exams[i].students.end())
      );
      _exams[i].students.clear();
      _exams[i].students.shrink_to_fit();
      for (int j = i + 1; j < num_exams; ++j) {
        int common_count = 0;
        for (const std::string& student_j : _exams[j].students) {
          common_count += students_i.contains(student_j);
        }
        conflict(i, j) = conflict(j, i) = common_count;
      }
    }
    return conflict;
  }()),
  student_conflicts(student_conflicts_matrix),
  total_student_conflicts([&]() {
    std::vector<int> totals(num_exams);
    for (int exam = 0; exam < num_exams; ++exam) {
      const auto row = student_conflicts[exam];
      totals[exam] = std::accumulate(row.begin(), row.end(), 0);
    }
    return totals;
  }()),
  exams(std::move(_exams))
{
#ifndef NDEBUG
  std::unordered_set<std::string> unique_checker;
  for (const Exam& e : exams) {
    utils::panic_if(!unique_checker.emplace(e.code).second,
                    "Duplicate exam code: {}", e.code);
  }
#endif // NDEBUG
}

double Evaluator::calculate_delta_penalty(
  const std::vector<int>& schedule,
  int exam,
  int new_slot,
  int ignore_exam
) const {
  const int cur_slot = schedule[exam];
  const int exam_credits = exams[exam].credits;
  double delta = 0.0;
  for (const auto& [conflict_exam, conflict_count] : student_conflicts[exam]) {
    const int conflict_slot = schedule[conflict_exam];
    if (conflict_exam == ignore_exam || conflict_slot == -1) {
      continue;
    }
    const int weight = conflict_count * (exam_credits + exams[conflict_exam].credits);
    delta += weight * proximity_penalties(new_slot, conflict_slot);
    if (cur_slot != -1) {
      delta -= weight * proximity_penalties(cur_slot, conflict_slot);
    }
  }
  return delta;
}

} // namespace common