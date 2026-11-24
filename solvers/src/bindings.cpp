#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <nanobind/nanobind.h>
#include <nanobind/nb_defs.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include "evaluation/params.hpp"
#include "exam.hpp"
#include "hyperparameters.hpp"
#include "problem_data.hpp"
#include "room.hpp"
#include "rooms/params.hpp"
#include "rooms/room_allocator.hpp"
#include "rooms/room_assignment.hpp"
#include "slots/aco/ant_colony.hpp"
#include "slots/aco/params.hpp"
#include "slots/local_search/local_search.hpp"
#include "slots/local_search/params.hpp"
#include "slots/slot_result.hpp"
#include "timetable.hpp"
#include "utils/vector.hpp"

namespace nb = nanobind;

namespace {

namespace evaluation = cetp::evaluation;
namespace rooms = cetp::rooms;
namespace slots = cetp::slots;
using AcoParams = slots::aco::Params;
using LsParams = slots::local_search::Params;

constexpr uint64_t DEFAULT_BASE_SEED = 42;

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

  nb::class_<cetp::ExamSection>(m, "ExamSection")
      .def(
          "__init__",
          [](cetp::ExamSection* self,
             std::string code,
             const std::vector<std::string>& students,
             const std::vector<std::string>& feasible_rooms) -> void {
            new (self)
                cetp::ExamSection(std::move(code), to_vector(students), to_vector(feasible_rooms));
          },
          nb::arg("code"),
          nb::arg("students"),
          nb::arg("feasible_rooms")
      )
      .def_ro("code", &cetp::ExamSection::code)
      .def_prop_ro(
          "students",
          [](const cetp::ExamSection& section) -> std::vector<std::string> {
            return to_std_vector(section.students);
          }
      )
      .def_prop_ro(
          "feasible_rooms",
          [](const cetp::ExamSection& section) -> std::vector<std::string> {
            return to_std_vector(section.feasible_rooms);
          }
      );

  nb::class_<cetp::Exam>(m, "Exam")
      .def(
          "__init__",
          [](cetp::Exam* self,
             std::string code,
             int credits,
             const std::vector<cetp::ExamSection>& sections,
             const std::vector<int64_t>& feasible_timestamps) -> void {
            new (self) cetp::Exam(
                std::move(code),
                credits,
                to_vector(sections),
                to_vector(feasible_timestamps)
            );
          },
          nb::arg("code"),
          nb::arg("credits"),
          nb::arg("sections"),
          nb::arg("feasible_timestamps")
      )
      .def_ro("code", &cetp::Exam::code)
      .def_ro("credits", &cetp::Exam::credits)
      .def_prop_ro(
          "sections",
          [](const cetp::Exam& exam) -> std::vector<cetp::ExamSection> {
            return to_std_vector(exam.sections);
          }
      )
      .def_prop_ro("feasible_timestamps", [](const cetp::Exam& exam) -> std::vector<int64_t> {
        return to_std_vector(exam.feasible_timestamps);
      });

  nb::class_<cetp::Room>(m, "Room")
      .def(
          nb::init<std::string, int, std::string, std::string>(),
          nb::arg("code"),
          nb::arg("capacity"),
          nb::arg("location") = "default",
          nb::arg("type") = "default"
      )
      .def_prop_rw("code", &cetp::Room::code, &cetp::Room::set_code)
      .def_prop_rw("capacity", &cetp::Room::capacity, &cetp::Room::set_capacity)
      .def_prop_rw("location", &cetp::Room::location, &cetp::Room::set_location)
      .def_prop_rw("type", &cetp::Room::type, &cetp::Room::set_type);

  nb::class_<evaluation::Params>(m, "EvalParams")
      .def(nb::init<>())
      .def_prop_rw(
          "penalty_decay_base",
          &evaluation::Params::penalty_decay_base,
          &evaluation::Params::set_penalty_decay_base
      );

  nb::class_<AcoParams>(m, "AcoParams")
      .def(nb::init<>())
      .def_prop_rw("num_iters", &AcoParams::num_iters, &AcoParams::set_num_iters)
      .def_prop_rw("num_ants", &AcoParams::num_ants, &AcoParams::set_num_ants)
      .def_prop_rw("alpha", &AcoParams::alpha, &AcoParams::set_alpha)
      .def_prop_rw("beta", &AcoParams::beta, &AcoParams::set_beta)
      .def_prop_rw("rho", &AcoParams::rho, &AcoParams::set_rho)
      .def_prop_rw("tau_min", &AcoParams::tau_min, &AcoParams::set_tau_min)
      .def_prop_rw("tau_max", &AcoParams::tau_max, &AcoParams::set_tau_max)
      .def_prop_rw("max_retries", &AcoParams::max_retries, &AcoParams::set_max_retries)
      .def("set_tau_bounds", &AcoParams::set_tau_bounds, nb::arg("tau_min"), nb::arg("tau_max"));

  nb::class_<LsParams>(m, "LsParams")
      .def(nb::init<>())
      .def_prop_rw("prob_1_move", &LsParams::prob_1_move, &LsParams::set_prob_1_move)
      .def_prop_rw("max_improvements", &LsParams::max_improvements, &LsParams::set_max_improvements)
      .def_prop_rw("patience", &LsParams::patience, &LsParams::set_patience);

  nb::class_<rooms::Params>(m, "RoomAssignmentParams")
      .def(nb::init<>())
      .def_prop_rw(
          "max_time_seconds",
          &rooms::Params::max_time_seconds,
          &rooms::Params::set_max_time_seconds
      )
      .def_prop_rw("num_workers", &rooms::Params::num_workers, &rooms::Params::set_num_workers)
      .def_prop_rw("random_seed", &rooms::Params::random_seed, &rooms::Params::set_random_seed);

  nb::class_<cetp::Hyperparams>(m, "Hyperparams")
      .def(
          nb::init<evaluation::Params, AcoParams, LsParams, rooms::Params>(),
          nb::arg("_eval") = evaluation::Params(),
          nb::arg("_aco") = AcoParams(),
          nb::arg("_ls") = LsParams(),
          nb::arg("_room_assignment") = rooms::Params()
      )
      .def_rw("eval", &cetp::Hyperparams::eval)
      .def_rw("aco", &cetp::Hyperparams::aco)
      .def_rw("ls", &cetp::Hyperparams::ls)
      .def_rw("room_assignment", &cetp::Hyperparams::room_assignment);

  nb::class_<cetp::ProblemData>(m, "ProblemData")
      .def(
          "__init__",
          [](cetp::ProblemData* self,
             const std::vector<cetp::Exam>& exams,
             const std::vector<cetp::Room>& rooms) -> void {
            new (self) cetp::ProblemData(to_vector(exams), to_vector(rooms));
          },
          nb::arg("exams"),
          nb::arg("rooms")
      )
      .def_ro("num_exams", &cetp::ProblemData::num_exams)
      .def_ro("num_slots", &cetp::ProblemData::num_slots)
      .def_ro("num_rooms", &cetp::ProblemData::num_rooms)
      .def_ro("num_sections", &cetp::ProblemData::num_sections)
      .def_ro("num_students", &cetp::ProblemData::num_students)
      .def_ro("num_enrollments", &cetp::ProblemData::num_enrollments)
      .def_prop_ro(
          "exam_codes",
          [](const cetp::ProblemData& data) -> std::vector<std::string> {
            return to_std_vector(data.exam_codes);
          }
      )
      .def_prop_ro(
          "section_codes",
          [](const cetp::ProblemData& data) -> std::vector<std::string> {
            return to_std_vector(data.section_codes);
          }
      )
      .def_prop_ro(
          "room_codes",
          [](const cetp::ProblemData& data) -> std::vector<std::string> {
            return to_std_vector(data.room_codes);
          }
      )
      .def_prop_ro("slot_timestamps", [](const cetp::ProblemData& data) -> std::vector<int64_t> {
        return to_std_vector(data.slot_timestamps);
      });

  nb::class_<slots::SlotResult>(m, "SlotResult")
      .def_prop_ro("found", &slots::SlotResult::found)
      .def_prop_ro(
          "schedule",
          [](const slots::SlotResult& result) -> std::vector<int> {
            return to_std_vector(result.schedule);
          }
      )
      .def_ro("penalty", &slots::SlotResult::penalty);

  nb::enum_<rooms::RoomAssignmentStatus>(m, "RoomAssignmentStatus")
      .value("OPTIMAL", rooms::RoomAssignmentStatus::OPTIMAL)
      .value("FEASIBLE", rooms::RoomAssignmentStatus::FEASIBLE)
      .value("INFEASIBLE", rooms::RoomAssignmentStatus::INFEASIBLE)
      .value("UNKNOWN", rooms::RoomAssignmentStatus::UNKNOWN)
      .value("MODEL_INVALID", rooms::RoomAssignmentStatus::MODEL_INVALID);

  nb::class_<rooms::RoomAssignment>(m, "RoomAssignment")
      .def_ro("status", &rooms::RoomAssignment::status)
      .def_prop_ro(
          "slot_statuses",
          [](const rooms::RoomAssignment& assignment) -> std::vector<rooms::RoomAssignmentStatus> {
            return to_std_vector(assignment.slot_statuses);
          }
      )
      .def_prop_ro("found", &rooms::RoomAssignment::found)
      .def_prop_ro("room_count", &rooms::RoomAssignment::room_count)
      .def_ro("extra_locations", &rooms::RoomAssignment::extra_locations)
      .def_ro("unused_capacity", &rooms::RoomAssignment::unused_capacity)
      .def(
          "rooms_for_section",
          [](const rooms::RoomAssignment& assignment, int section) -> std::vector<int> {
            const auto rooms = assignment.rooms_for_section(section);
            std::vector<int> result;
            result.reserve(size_t(rooms.size()));
            for (int room : rooms) {
              result.push_back(room);
            }
            return result;
          },
          nb::arg("section_id")
      );

  nb::class_<cetp::TimetableResult>(m, "TimetableResult")
      .def_ro("slots", &cetp::TimetableResult::slots)
      .def_ro("rooms", &cetp::TimetableResult::rooms);

  m.def(
      "solve_slots_aco",
      [](const cetp::ProblemData& problem,
         AcoParams aco_params,
         evaluation::Params eval_params,
         uint64_t base_seed,
         const slots::SlotProgressCallback& callback) -> slots::SlotResult {
        const nb::gil_scoped_release release;
        return slots::aco::solve(problem, aco_params, eval_params, base_seed, callback);
      },
      nb::arg("problem"),
      nb::arg("aco_params"),
      nb::arg("eval_params"),
      nb::kw_only(),
      nb::arg("base_seed") = DEFAULT_BASE_SEED,
      nb::arg("callback") = nb::none()
  );

  m.def(
      "improve_slots",
      [](const cetp::ProblemData& problem,
         std::vector<int> schedule,
         LsParams ls_params,
         evaluation::Params eval_params,
         uint64_t base_seed) -> slots::SlotResult {
        const nb::gil_scoped_release release;
        return slots::local_search::improve(
            problem,
            Vector<int>(std::move(schedule)),
            ls_params,
            eval_params,
            base_seed
        );
      },
      nb::arg("problem"),
      nb::arg("schedule"),
      nb::arg("ls_params"),
      nb::arg("eval_params"),
      nb::kw_only(),
      nb::arg("base_seed") = DEFAULT_BASE_SEED
  );

  m.def(
      "assign_rooms",
      [](const cetp::ProblemData& problem,
         std::vector<int> schedule,
         rooms::Params room_params) -> rooms::RoomAssignment {
        const nb::gil_scoped_release release;
        return rooms::assign_rooms(problem, Vector<int>(std::move(schedule)), room_params);
      },
      nb::arg("problem"),
      nb::arg("schedule"),
      nb::arg("room_params")
  );

  m.def(
      "solve_timetable",
      [](const cetp::ProblemData& problem,
         cetp::Hyperparams hyperparams,
         uint64_t base_seed,
         const slots::SlotProgressCallback& callback) -> cetp::TimetableResult {
        const nb::gil_scoped_release release;
        return cetp::solve_timetable(problem, hyperparams, base_seed, callback);
      },
      nb::arg("problem"),
      nb::arg("hyperparams"),
      nb::kw_only(),
      nb::arg("base_seed") = DEFAULT_BASE_SEED,
      nb::arg("callback") = nb::none()
  );
}
