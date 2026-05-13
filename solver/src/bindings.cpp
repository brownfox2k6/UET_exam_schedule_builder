#include <pybind11/attr.h>
#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "aco/ant.hpp"
#include "aco/ant_colony.hpp"
#include "common/hyperparameters.hpp"

namespace py = pybind11;

PYBIND11_MODULE(aco_solver, m) {
  m.doc() = "Solver for University Examination Timetabling "
            "using Ant Colony Optimization and Local Search";

  py::class_<common::Hyperparams::Evaluation>(m, "EvalParams")
    .def(py::init<double>(), py::arg("_penalty_decay_base") = 2.0)
    .def_readwrite("penalty_decay_base", &common::Hyperparams::Evaluation::penalty_decay_base);

  py::class_<common::Hyperparams::ACO>(m, "AcoParams")
    .def(py::init<int, int, double, double, double, double, double>(),
         py::arg("_num_iters") = 100,
         py::arg("_num_ants") = 10,
         py::arg("_alpha") = 1.0,
         py::arg("_beta") = 2.0,
         py::arg("_rho") = 0.1,
         py::arg("_tau_min") = 0.01,
         py::arg("_tau_max") = 1.0)
    .def_readwrite("num_iters",          &common::Hyperparams::ACO::num_iters)
    .def_readwrite("num_ants",           &common::Hyperparams::ACO::num_ants)
    .def_readwrite("alpha",              &common::Hyperparams::ACO::alpha)
    .def_readwrite("beta",               &common::Hyperparams::ACO::beta)
    .def_readwrite("rho",                &common::Hyperparams::ACO::rho)
    .def_readwrite("tau_min",            &common::Hyperparams::ACO::tau_min)
    .def_readwrite("tau_max",            &common::Hyperparams::ACO::tau_max);

  py::class_<common::Hyperparams::LS>(m, "LsParams")
    .def(py::init<double, int, int>(),
         py::arg("_prob_1_move") = 0.5, 
         py::arg("_max_improvements") = 10, 
         py::arg("_patience") = 100)
    .def_readwrite("prob_1_move",      &common::Hyperparams::LS::prob_1_move)
    .def_readwrite("max_improvements", &common::Hyperparams::LS::max_improvements)
    .def_readwrite("patience",         &common::Hyperparams::LS::patience);

  py::class_<common::Hyperparams>(m, "Hyperparams")
    .def(py::init<common::Hyperparams::Evaluation, common::Hyperparams::ACO, common::Hyperparams::LS>(),
         py::arg("_eval") = common::Hyperparams::Evaluation(),
         py::arg("_aco") = common::Hyperparams::ACO(),
         py::arg("_ls") = common::Hyperparams::LS())
    .def_readwrite("aco", &common::Hyperparams::aco)
    .def_readwrite("ls",  &common::Hyperparams::ls);

  py::class_<aco::Ant>(m, "Ant")
    .def_readonly("schedule", &aco::Ant::schedule)
    .def_readonly("fitness",  &aco::Ant::fitness);

  py::class_<aco::AntColony>(m, "AntColony")
    .def(py::init([](const common::Hyperparams& hp, 
                     const std::vector<std::vector<int>>& conflict_data, 
                     const std::vector<int64_t>& timestamps,
                     int base_seed = -1) {
      common::Matrix<int> conflict_mat(conflict_data);
      return new aco::AntColony(hp, conflict_mat, timestamps, base_seed);
    }))
    .def("run_one_iteration", &aco::AntColony::run_one_iteration)
    .def("run", &aco::AntColony::run, py::call_guard<py::gil_scoped_release>())
    .def_readonly("global_best", &aco::AntColony::global_best);
}