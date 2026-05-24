#include <cstdint>
#include <pybind11/attr.h>
#include <pybind11/cast.h>
#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "aco/ant_colony.hpp"
#include "common/hyperparameters.hpp"
#include "common/solution.hpp"
#include "common/exam.hpp"

namespace py = pybind11;

PYBIND11_MODULE(aco_solver, m) {
  m.doc() = "Solver for University Examination Timetabling "
            "using Ant Colony Optimization and Local Search";

  py::class_<common::Exam>(m, "Exam")
    .def(py::init<>())
    .def(py::init<std::string, int, std::vector<std::string>, std::vector<int>, std::vector<int>, std::vector<int>>(),
         py::arg("_code"),
         py::arg("_credits"),
         py::arg("_students"),
         py::arg("_feasible_slots"),
         py::arg("_feasible_rooms"),
         py::arg("_feasible_proctors"))
    .def_readwrite("code",              &common::Exam::code)
    .def_readwrite("credits",           &common::Exam::credits)
    .def_readonly("student_count",      &common::Exam::student_count)
    .def_readwrite("students",          &common::Exam::students)
    .def_readwrite("feasible_slots",    &common::Exam::feasible_slots)
    .def_readwrite("feasible_rooms",    &common::Exam::feasible_rooms)
    .def_readwrite("feasible_proctors", &common::Exam::feasible_proctors);

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
         py::arg_v("_eval", common::Hyperparams::Evaluation(), "EvalParams()"),
         py::arg_v("_aco", common::Hyperparams::ACO(), "AcoParams()"),
         py::arg_v("_ls", common::Hyperparams::LS(), "LsParams()"))
    .def_readwrite("eval", &common::Hyperparams::eval)
    .def_readwrite("aco",  &common::Hyperparams::aco)
    .def_readwrite("ls",   &common::Hyperparams::ls);

  py::class_<common::Solution>(m, "Solution")
    .def_readonly("schedule", &common::Solution::schedule)
    .def_readonly("fitness",  &common::Solution::fitness);

  py::class_<aco::AntColony>(m, "AntColony")
    .def(py::init([](const common::Hyperparams& hp, 
                     const std::vector<std::vector<int>>& conflict_data, 
                     const std::vector<int64_t>& timestamps,
                     int64_t _base_seed = -1) {
      common::Matrix<int> conflict_mat(conflict_data);
      return aco::AntColony(hp, conflict_mat, timestamps, _base_seed);
    }))
    .def("run_one_iteration", &aco::AntColony::run_one_iteration, py::call_guard<py::gil_scoped_release>())
    .def("run", &aco::AntColony::run)
    .def_readonly("global_best_schedule", &aco::AntColony::global_best_schedule)
    .def_readonly("global_best_fitness", &aco::AntColony::global_best_fitness);
}