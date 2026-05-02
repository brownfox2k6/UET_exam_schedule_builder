#pragma once

namespace aco {

/**
  * @brief Hyperparameters controlling the Ant Colony Optimization technique.
  */
struct Hyperparams {
  int num_iters;  // Number of iterations
  int num_ants;   // Number of ants used in each iteration
  double alpha;   // Pheromone importance in selection probability (tau ^ alpha)
  double beta;    // Heuristic importance in selection probability (eta ^ beta)
  double rho;     // Pheromone evaporation rate for global update
  double tau_min; // Minimum pheromone trail limit
  double tau_max; // Maximum pheromone trail limit

  Hyperparams()
    : num_iters(100), num_ants(10), alpha(1.0), beta(2.0),
      rho(0.1), tau_min(0.01), tau_max(1.0) {}

  Hyperparams(int iters, int ants, double a, double b, double r, double t_min, double t_max)
    : num_iters(iters), num_ants(ants), alpha(a), beta(b),
      rho(r), tau_min(t_min), tau_max(t_max) {}
};

} // namespace aco


namespace ls {

/**
* @brief Hyperparameters controlling the Local Search behavior.
*/
struct Hyperparams {
  double prob_1_move;   // Probability of applying 1-move operator instead of 2-swap
  int max_improvements; // Maximum number of actual successful moves (downhill steps)
  int patience;         // Stop early after this many consecutive non-improving attempts

  Hyperparams()
    : prob_1_move(0.5), max_improvements(10), patience(100) {}

  Hyperparams(double p1m, int m_improve, int m_fails)
    : prob_1_move(p1m), max_improvements(m_improve), patience(m_fails) {}
};

} // namespace ls


namespace common {
  
/**
  * @brief A unified container for all algorithmic hyperparameters.
  */
struct Hyperparams {
  aco::Hyperparams aco;
  ls::Hyperparams ls;
};

} // namespace common