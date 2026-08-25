#include <nanobind/stl/function.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstdint>

#include "aco/ant_colony.hpp"
#include "common/exam.hpp"
#include "common/hyperparameters.hpp"
#include "common/room.hpp"
#include "common/solution.hpp"

namespace nb = nanobind;

NB_MODULE(aco_solver, m) {  // NOLINT(readability-identifier-length)
  m.doc() =
      "Solver for University Examination Timetabling "
      "using Ant Colony Optimization and Local Search";

  nb::class_<common::ExamSection>(m, "ExamSection")
      .def(nb::init<std::string, std::vector<std::string>>(), nb::arg("code"), nb::arg("students"))
      .def_prop_rw("code", &common::ExamSection::code, &common::ExamSection::set_code)
      .def_prop_rw("students", &common::ExamSection::students, &common::ExamSection::set_students)
      .def_prop_ro("student_count", &common::ExamSection::student_count);

  nb::class_<common::Exam>(m, "Exam")
      .def(nb::init<std::string, int, std::vector<common::ExamSection>, std::vector<int>,
                    std::vector<int>>(),
           nb::arg("code"), nb::arg("credits"), nb::arg("sections"), nb::arg("feasible_slots"),
           nb::arg("feasible_rooms"))
      .def_prop_rw("code", &common::Exam::code, &common::Exam::set_code)
      .def_prop_rw("credits", &common::Exam::credits, &common::Exam::set_credits)
      .def_prop_rw("sections", &common::Exam::sections, &common::Exam::set_sections)
      .def_prop_rw("feasible_slots", &common::Exam::feasible_slots,
                   &common::Exam::set_feasible_slots)
      .def_prop_rw("feasible_rooms", &common::Exam::feasible_rooms,
                   &common::Exam::set_feasible_rooms)
      .def_prop_ro("section_count", &common::Exam::section_count)
      .def_prop_ro("student_count", &common::Exam::student_count);

  nb::class_<common::Room>(m, "Room")
      .def(nb::init<std::string, int, std::string, std::string>(), nb::arg("code"),
           nb::arg("capacity"), nb::arg("location") = "default", nb::arg("type") = "default")
      .def_prop_rw("code", &common::Room::code, &common::Room::set_code)
      .def_prop_rw("capacity", &common::Room::capacity, &common::Room::set_capacity)
      .def_prop_rw("location", &common::Room::location, &common::Room::set_location)
      .def_prop_rw("type", &common::Room::type, &common::Room::set_type);

  nb::class_<common::Hyperparams::Evaluation>(m, "EvalParams")
      .def(nb::init<>())
      .def_prop_rw("penalty_decay_base", &common::Hyperparams::Evaluation::penalty_decay_base,
                   &common::Hyperparams::Evaluation::set_penalty_decay_base);

  nb::class_<common::Hyperparams::ACO>(m, "AcoParams")
      .def(nb::init<>())
      .def_prop_rw("num_iters", &common::Hyperparams::ACO::num_iters,
                   &common::Hyperparams::ACO::set_num_iters)
      .def_prop_rw("num_ants", &common::Hyperparams::ACO::num_ants,
                   &common::Hyperparams::ACO::set_num_ants)
      .def_prop_rw("alpha", &common::Hyperparams::ACO::alpha, &common::Hyperparams::ACO::set_alpha)
      .def_prop_rw("beta", &common::Hyperparams::ACO::beta, &common::Hyperparams::ACO::set_beta)
      .def_prop_rw("rho", &common::Hyperparams::ACO::rho, &common::Hyperparams::ACO::set_rho)
      .def_prop_rw("tau_min", &common::Hyperparams::ACO::tau_min,
                   &common::Hyperparams::ACO::set_tau_min)
      .def_prop_rw("tau_max", &common::Hyperparams::ACO::tau_max,
                   &common::Hyperparams::ACO::set_tau_max)
      .def_prop_rw("max_retries", &common::Hyperparams::ACO::max_retries,
                   &common::Hyperparams::ACO::set_max_retries)
      .def("set_tau_bounds", &common::Hyperparams::ACO::set_tau_bounds, nb::arg("tau_min"),
           nb::arg("tau_max"));

  nb::class_<common::Hyperparams::LS>(m, "LsParams")
      .def(nb::init<>())
      .def_prop_rw("prob_1_move", &common::Hyperparams::LS::prob_1_move,
                   &common::Hyperparams::LS::set_prob_1_move)
      .def_prop_rw("max_improvements", &common::Hyperparams::LS::max_improvements,
                   &common::Hyperparams::LS::set_max_improvements)
      .def_prop_rw("patience", &common::Hyperparams::LS::patience,
                   &common::Hyperparams::LS::set_patience);

  nb::class_<common::Hyperparams>(m, "Hyperparams")
      .def(nb::init<common::Hyperparams::Evaluation, common::Hyperparams::ACO,
                    common::Hyperparams::LS>(),
           nb::arg("_eval") = common::Hyperparams::Evaluation(),
           nb::arg("_aco") = common::Hyperparams::ACO(),
           nb::arg("_ls") = common::Hyperparams::LS())
      .def_rw("eval", &common::Hyperparams::eval)
      .def_rw("aco", &common::Hyperparams::aco)
      .def_rw("ls", &common::Hyperparams::ls);

  nb::class_<common::Solution>(m, "Solution")
      .def_ro("schedule", &common::Solution::assigned_slots)
      .def_ro("fitness", &common::Solution::fitness);

  nb::class_<aco::AntColony>(m, "AntColony")
      .def(nb::init<common::Hyperparams, const std::vector<common::Exam>&, std::vector<int64_t>,
                    const std::vector<common::Room>&, uint64_t>(),
           nb::arg("hyperparams"), nb::arg("exams"), nb::arg("slot_timestamps"), nb::arg("rooms"),
           nb::arg("base_seed") = 42)  // NOLINT(*-magic-numbers)
      .def("run_one_iteration", &aco::AntColony::run_one_iteration,
           nb::call_guard<nb::gil_scoped_release>())
      .def("run", &aco::AntColony::run, nb::arg("callback") = nb::none(),
           nb::call_guard<nb::gil_scoped_release>())
      .def_ro("global_best_schedule", &aco::AntColony::global_best_schedule)
      .def_ro("global_best_fitness", &aco::AntColony::global_best_fitness);
}