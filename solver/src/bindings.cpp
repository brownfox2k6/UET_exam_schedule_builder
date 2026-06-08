#include <pybind11/attr.h>
#include <pybind11/cast.h>
#include <pybind11/functional.h>
#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <memory>

#include "aco/ant_colony.hpp"
#include "common/exam.hpp"
#include "common/hyperparameters.hpp"
#include "common/room.hpp"
#include "common/solution.hpp"

namespace py = pybind11;

PYBIND11_MODULE(aco_solver, m) {  // NOLINT(readability-identifier-length)
  m.doc() =
      "Solver for University Examination Timetabling "
      "using Ant Colony Optimization and Local Search";

  py::class_<common::ExamSection>(m, "ExamSection")
      .def(py::init<std::string, std::vector<std::string>>(), py::arg("code"), py::arg("students"))
      .def_property("code", &common::ExamSection::code, &common::ExamSection::set_code)
      .def_property("students", &common::ExamSection::students, &common::ExamSection::set_students)
      .def_property_readonly("student_count", &common::ExamSection::student_count);

  py::class_<common::Exam>(m, "Exam")
      .def(py::init<std::string, int, std::vector<common::ExamSection>, std::vector<int>,
                    std::vector<int>>(),
           py::arg("code"), py::arg("credits"), py::arg("sections"), py::arg("feasible_slots"),
           py::arg("feasible_rooms"))
      .def_property("code", &common::Exam::code, &common::Exam::set_code)
      .def_property("credits", &common::Exam::credits, &common::Exam::set_credits)
      .def_property("sections", &common::Exam::sections, &common::Exam::set_sections)
      .def_property("feasible_slots", &common::Exam::feasible_slots,
                    &common::Exam::set_feasible_slots)
      .def_property("feasible_rooms", &common::Exam::feasible_rooms,
                    &common::Exam::set_feasible_rooms)
      .def_property_readonly("section_count", &common::Exam::section_count)
      .def_property_readonly("student_count", &common::Exam::student_count);

  py::class_<common::Room>(m, "Room")
      .def(py::init<std::string, int, std::string, std::string>(), py::arg("code"),
           py::arg("capacity"), py::arg("location") = "default", py::arg("type") = "default")
      .def_property("code", &common::Room::code, &common::Room::set_code)
      .def_property("capacity", &common::Room::capacity, &common::Room::set_capacity)
      .def_property("location", &common::Room::location, &common::Room::set_location)
      .def_property("type", &common::Room::type, &common::Room::set_type);

  py::class_<common::Hyperparams::Evaluation>(m, "EvalParams")
      .def(py::init<>())
      .def_property("penalty_decay_base", &common::Hyperparams::Evaluation::penalty_decay_base,
                    &common::Hyperparams::Evaluation::set_penalty_decay_base);

  py::class_<common::Hyperparams::ACO>(m, "AcoParams")
      .def(py::init<>())
      .def_property("num_iters", &common::Hyperparams::ACO::num_iters,
                    &common::Hyperparams::ACO::set_num_iters)
      .def_property("num_ants", &common::Hyperparams::ACO::num_ants,
                    &common::Hyperparams::ACO::set_num_ants)
      .def_property("alpha", &common::Hyperparams::ACO::alpha, &common::Hyperparams::ACO::set_alpha)
      .def_property("beta", &common::Hyperparams::ACO::beta, &common::Hyperparams::ACO::set_beta)
      .def_property("rho", &common::Hyperparams::ACO::rho, &common::Hyperparams::ACO::set_rho)
      .def_property("tau_min", &common::Hyperparams::ACO::tau_min,
                    &common::Hyperparams::ACO::set_tau_min)
      .def_property("tau_max", &common::Hyperparams::ACO::tau_max,
                    &common::Hyperparams::ACO::set_tau_max)
      .def_property("max_retries", &common::Hyperparams::ACO::max_retries,
                    &common::Hyperparams::ACO::set_max_retries)
      .def("set_tau_bounds", &common::Hyperparams::ACO::set_tau_bounds, py::arg("tau_min"),
           py::arg("tau_max"));

  py::class_<common::Hyperparams::LS>(m, "LsParams")
      .def(py::init<>())
      .def_property("prob_1_move", &common::Hyperparams::LS::prob_1_move,
                    &common::Hyperparams::LS::set_prob_1_move)
      .def_property("max_improvements", &common::Hyperparams::LS::max_improvements,
                    &common::Hyperparams::LS::set_max_improvements)
      .def_property("patience", &common::Hyperparams::LS::patience,
                    &common::Hyperparams::LS::set_patience);

  py::class_<common::Hyperparams>(m, "Hyperparams")
      .def(py::init<common::Hyperparams::Evaluation, common::Hyperparams::ACO,
                    common::Hyperparams::LS>(),
           py::arg_v("_eval", common::Hyperparams::Evaluation(), "EvalParams()"),
           py::arg_v("_aco", common::Hyperparams::ACO(), "AcoParams()"),
           py::arg_v("_ls", common::Hyperparams::LS(), "LsParams()"))
      .def_readwrite("eval", &common::Hyperparams::eval)
      .def_readwrite("aco", &common::Hyperparams::aco)
      .def_readwrite("ls", &common::Hyperparams::ls);

  py::class_<common::Solution>(m, "Solution")
      .def_readonly("schedule", &common::Solution::assigned_slots)
      .def_readonly("fitness", &common::Solution::fitness);

  py::class_<aco::AntColony>(m, "AntColony")
      .def(py::init([](common::Hyperparams hyperparams, const std::vector<common::Exam>& exams,
                       std::vector<int64_t> slot_timestamps, const std::vector<common::Room>& rooms,
                       int64_t base_seed) -> std::unique_ptr<aco::AntColony> {
             return std::make_unique<aco::AntColony>(hyperparams, exams, std::move(slot_timestamps),
                                                     rooms, base_seed);
           }),
           py::arg("hyperparams"), py::arg("exams"), py::arg("slot_timestamps"), py::arg("rooms"),
           py::arg("base_seed") = 42)  // NOLINT(readability-magic-numbers)
      .def("run_one_iteration", &aco::AntColony::run_one_iteration,
           py::call_guard<py::gil_scoped_release>())
      .def("run", &aco::AntColony::run, py::arg("callback") = py::none())
      .def_readonly("global_best_schedule", &aco::AntColony::global_best_schedule)
      .def_readonly("global_best_fitness", &aco::AntColony::global_best_fitness);
}