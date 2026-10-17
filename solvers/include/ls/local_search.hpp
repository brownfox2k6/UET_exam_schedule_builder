#pragma once

#include <cstdint>
#include <mutex>
#include <random>
#include <vector>

#include "common/evaluator.hpp"
#include "common/hyperparameters.hpp"
#include "common/problem_data.hpp"
#include "common/solution.hpp"

namespace ls {

/**
 * @brief Standalone local-search algorithm for improving an existing timetable.
 *
 * The input schedule can come from ACO or any other solver. LocalSearch has no
 * dependency on how that initial timetable was constructed.
 */
class LocalSearch {
 public:
  LocalSearch(common::Hyperparams::Evaluation evaluation_params,
              common::Hyperparams::LS hyperparams, const std::vector<common::Exam>& exams,
              std::vector<int64_t> slot_timestamps, const std::vector<common::Room>& rooms,
              uint64_t base_seed = 42);  // NOLINT(*-magic-numbers)

  /** @brief Improves a feasible initial schedule and returns its final schedule and penalty. */
  auto run(const std::vector<int>& initial_schedule) -> common::Solution;

 private:
  const common::Hyperparams::Evaluation evaluation_params_;
  const common::Hyperparams::LS hyperparams_;
  const common::ProblemData problem_data_;  // NOLINT(*const-or-ref-data-members*)
  const common::Evaluator evaluator_;       // NOLINT(*const-or-ref-data-members*)
  common::Solution solution_;
  std::mt19937 rng_;
  std::mutex execution_mutex_;

  void initialize_solution(const std::vector<int>& initial_schedule);
  auto go_1_move() -> bool;
  auto go_2_swap() -> bool;
};

}  // namespace ls