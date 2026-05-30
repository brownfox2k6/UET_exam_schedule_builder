#pragma once

namespace common {
  
/**
 * @brief A unified container for all algorithmic hyperparameters.
 */
struct Hyperparams {

  /**
   * @brief Parameters related to the problem evaluation and cost function.
   */
  struct Evaluation {
    // Base for the exponential decay of time-gap penalties (base ^ -delta_days).
    // Controls how fast the penalty drops: 2.0 halves the penalty each day.
    double penalty_decay_base;

    Evaluation(
      double _penalty_decay_base = 2.0
    ):
      penalty_decay_base(_penalty_decay_base)
    {}
  }; // struct Evaluation

  /**
   * @brief Hyperparameters controlling the Ant Colony Optimization technique.
   */
  struct ACO {
    int num_iters;  // Number of iterations
    int num_ants;   // Number of ants used in each iteration
    double alpha;   // Pheromone importance in selection probability (tau ^ alpha)
    double beta;    // Heuristic importance in selection probability (eta ^ beta)
    double rho;     // Pheromone evaporation rate for global update
    double tau_min; // Minimum pheromone trail limit
    double tau_max; // Maximum pheromone trail limit

    ACO(
      int _num_iters = 100,
      int _num_ants = 10,
      double _alpha = 1.0,
      double _beta = 2.0,
      double _rho = 0.1,
      double _tau_min = 0.01,
      double _tau_max = 1.0
    ):
      num_iters(_num_iters),
      num_ants(_num_ants),
      alpha(_alpha),
      beta(_beta),
      rho(_rho),
      tau_min(_tau_min),
      tau_max(_tau_max)
    {}
  }; // struct ACO

  /**
   * @brief Hyperparameters controlling the Local Search behavior.
   */
  struct LS {
    double prob_1_move;   // Probability of applying 1-move operator instead of 2-swap
    int max_improvements; // Maximum number of actual successful moves (downhill steps)
    int patience;         // Stop early after this many consecutive non-improving attempts

    LS(
      double _prob_1_move = 0.5,
      int _max_improvements = 10,
      int _patience = 100
    ):
      prob_1_move(_prob_1_move),
      max_improvements(_max_improvements),
      patience(_patience)
    {}
  }; // struct LS

  Evaluation eval;
  ACO aco;
  LS ls;

  Hyperparams(
    Evaluation _eval = Evaluation(),
    ACO _aco = ACO(),
    LS _ls = LS()
  ):
    eval(_eval),
    aco(_aco),
    ls(_ls)
  {}

}; // struct Hyperparams

} // namespace common