#include <nanobind/stl/function.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <new>
#include <utility>

#include "aco/ant_colony.hpp"
#include "common/exam.hpp"
#include "common/hyperparameters.hpp"
#include "common/room.hpp"
#include "common/solution.hpp"
#include "ls/local_search.hpp"

namespace nb = nanobind;

namespace {

template <typename T>
auto to_vector(const std::vector<T>& values) -> Vector<T> {
  return {values.begin(), values.end()};
}

template <typename T>
auto to_std_vector(const Vector<T>& values) -> std::vector<T> {
  return {values.begin(), values.end()};
}

}  // namespace

// NOLINTNEXTLINE(readability-identifier-length, performance-unnecessary-value-param)
NB_MODULE(cetp_solvers, m) {
  m.doc() =
      "Some metaheuristic approaches for the Capacitated Examination Timetabling Problem (CETP).";

  nb::class_<common::ExamSection>(m, "ExamSection")
      .def(
          "__init__",
          [](common::ExamSection* self, std::string code, const std::vector<std::string>& students,
             const std::vector<int>& feasible_rooms) -> void {
            new (self) common::ExamSection(std::move(code), to_vector(students),
                                           to_vector(feasible_rooms));
          },
          nb::arg("code"), nb::arg("students"), nb::arg("feasible_rooms"))
      .def_prop_rw("code", &common::ExamSection::code, &common::ExamSection::set_code)
      .def_prop_rw(
          "students",
          [](const common::ExamSection& section) -> std::vector<std::string> {
            return to_std_vector(section.students());
          },
          [](common::ExamSection& section, const std::vector<std::string>& students) -> void {
            section.set_students(to_vector(students));
          })
      .def_prop_rw(
          "feasible_rooms",
          [](const common::ExamSection& section) -> std::vector<int> {
            return to_std_vector(section.feasible_rooms());
          },
          [](common::ExamSection& section, const std::vector<int>& feasible_rooms) -> void {
            section.set_feasible_rooms(to_vector(feasible_rooms));
          })
      .def_prop_ro("student_count", &common::ExamSection::student_count);

  nb::class_<common::Exam>(m, "Exam")
      .def(
          "__init__",
          [](common::Exam* self, std::string code, int credits,
             const std::vector<common::ExamSection>& sections,
             const std::vector<int>& feasible_slots) -> void {
            new (self) common::Exam(std::move(code), credits, to_vector(sections),
                                    to_vector(feasible_slots));
          },
          nb::arg("code"), nb::arg("credits"), nb::arg("sections"), nb::arg("feasible_slots"))
      .def_prop_rw("code", &common::Exam::code, &common::Exam::set_code)
      .def_prop_rw("credits", &common::Exam::credits, &common::Exam::set_credits)
      .def_prop_rw(
          "sections", [](const common::Exam& exam) { return to_std_vector(exam.sections()); },
          [](common::Exam& exam, const std::vector<common::ExamSection>& sections) -> void {
            exam.set_sections(to_vector(sections));
          })
      .def_prop_rw(
          "feasible_slots",
          [](const common::Exam& exam) -> std::vector<int> {
            return to_std_vector(exam.feasible_slots());
          },
          [](common::Exam& exam, const std::vector<int>& feasible_slots) -> void {
            exam.set_feasible_slots(to_vector(feasible_slots));
          })
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

  nb::class_<common::Hyperparams::RoomAssignment>(m, "RoomAssignmentParams")
      .def(nb::init<>())
      .def_prop_rw("max_attempts", &common::Hyperparams::RoomAssignment::max_attempts,
                   &common::Hyperparams::RoomAssignment::set_max_attempts);

  nb::class_<common::Hyperparams>(m, "Hyperparams")
      .def(nb::init<common::Hyperparams::Evaluation, common::Hyperparams::ACO,
                    common::Hyperparams::LS, common::Hyperparams::RoomAssignment>(),
           nb::arg("_eval") = common::Hyperparams::Evaluation(),
           nb::arg("_aco") = common::Hyperparams::ACO(), nb::arg("_ls") = common::Hyperparams::LS(),
           nb::arg("_room_assignment") = common::Hyperparams::RoomAssignment())
      .def_rw("eval", &common::Hyperparams::eval)
      .def_rw("aco", &common::Hyperparams::aco)
      .def_rw("ls", &common::Hyperparams::ls)
      .def_rw("room_assignment", &common::Hyperparams::room_assignment);

  nb::class_<common::Solution>(m, "Solution")
      .def_prop_ro("schedule",
                   [](const common::Solution& solution) {
                     return to_std_vector(solution.slot_assignment.assigned_slots);
                   })
      .def_ro("penalty", &common::Solution::penalty);

  nb::class_<aco::AntColony>(m, "AntColony")
      .def(
          "__init__",
          [](aco::AntColony* self, common::Hyperparams hyperparams,
             const std::vector<common::Exam>& exams, const std::vector<int64_t>& slot_timestamps,
             const std::vector<common::Room>& rooms, uint64_t base_seed) {
            new (self) aco::AntColony(hyperparams, to_vector(exams), to_vector(slot_timestamps),
                                      to_vector(rooms), base_seed);
          },
          nb::arg("hyperparams"), nb::arg("exams"), nb::arg("slot_timestamps"), nb::arg("rooms"),
          nb::arg("base_seed") = 42)  // NOLINT(*-magic-numbers)
      .def("run_one_iteration", &aco::AntColony::run_one_iteration,
           nb::call_guard<nb::gil_scoped_release>())
      .def("run", &aco::AntColony::run, nb::arg("callback") = nb::none(),
           nb::call_guard<nb::gil_scoped_release>())
      .def_prop_ro(
          "global_best_schedule",
          [](const aco::AntColony& colony) { return to_std_vector(colony.global_best_schedule); })
      .def_ro("global_best_penalty", &aco::AntColony::global_best_penalty);

  nb::class_<ls::LocalSearch>(m, "LocalSearch")
      .def(
          "__init__",
          [](ls::LocalSearch* self, common::Hyperparams::Evaluation eval_params,
             common::Hyperparams::LS ls_params, const std::vector<common::Exam>& exams,
             const std::vector<int64_t>& slot_timestamps, const std::vector<common::Room>& rooms,
             uint64_t base_seed) -> void {
            new (self) ls::LocalSearch(eval_params, ls_params, to_vector(exams),
                                       to_vector(slot_timestamps), to_vector(rooms), base_seed);
          },
          nb::arg("eval_params"), nb::arg("ls_params"), nb::arg("exams"),
          nb::arg("slot_timestamps"), nb::arg("rooms"),
          nb::arg("base_seed") = 42)  // NOLINT(*-magic-numbers)
      .def(
          "run",
          [](ls::LocalSearch& search, const std::vector<int>& initial_schedule)
              -> common::Solution { return search.run(to_vector(initial_schedule)); },
          nb::arg("initial_schedule"), nb::call_guard<nb::gil_scoped_release>());
}
