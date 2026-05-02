#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "ant.hpp"
#include "ant_colony.hpp"
#include "hyperparameters.hpp"

namespace py = pybind11;

PYBIND11_MODULE(aco_solver, m) {
  m.doc() = "Solver for University Examination Timetabling "
            "using Ant Colony Optimization and Local Search";

  py::class_<aco::Hyperparams>(m, "AcoParams")
    .def(py::init<>())
    .def_readwrite("num_iters", &aco::Hyperparams::num_iters)
    .def_readwrite("num_ants",  &aco::Hyperparams::num_ants)
    .def_readwrite("alpha",     &aco::Hyperparams::alpha)
    .def_readwrite("beta",      &aco::Hyperparams::beta)
    .def_readwrite("rho",       &aco::Hyperparams::rho)
    .def_readwrite("tau_min",   &aco::Hyperparams::tau_min)
    .def_readwrite("tau_max",   &aco::Hyperparams::tau_max);

  py::class_<ls::Hyperparams>(m, "LsParams")
    .def(py::init<>())
    .def_readwrite("prob_1_move",      &ls::Hyperparams::prob_1_move)
    .def_readwrite("max_improvements", &ls::Hyperparams::max_improvements)
    .def_readwrite("patience",         &ls::Hyperparams::patience);

  py::class_<common::Hyperparams>(m, "Hyperparams")
    .def(py::init<>())
    .def_readwrite("aco", &common::Hyperparams::aco)
    .def_readwrite("ls",  &common::Hyperparams::ls);

  py::class_<aco::Ant>(m, "Ant")
    .def_readonly("schedule", &aco::Ant::schedule)
    .def_readonly("fitness",  &aco::Ant::fitness);

  py::class_<aco::AntColony>(m, "AntColony")
    .def(py::init([](int num_exams, int num_slots, 
                  const common::Hyperparams& hp, 
                  const std::vector<std::vector<int>>& conflict_data, 
                  const std::vector<double>& abs_slots) {
      common::Matrix<int> conflict_mat(conflict_data);
      return new aco::AntColony(num_exams, num_slots, hp, conflict_mat, abs_slots);
    }))
    .def("run", &aco::AntColony::run)
    .def_readonly("global_best", &aco::AntColony::global_best);
}