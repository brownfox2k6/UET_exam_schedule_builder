#include <cstdint>
#include <string>

#include <nanobind/nanobind.h>
#include <nanobind/nb_defs.h>
#include <nanobind/stl/detail/nb_list.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>

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
#include "utils/span.hpp"
#include "utils/vector.hpp"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/** Converts Python sequences/lists directly to and from `Vector` (avoid `std::vector`) */
template <typename Type, typename Allocator>
struct type_caster<Vector<Type, Allocator>> : list_caster<Vector<Type, Allocator>, Type> {};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

namespace nb = nanobind;

namespace {

constexpr uint64_t DEFAULT_BASE_SEED = 42;

namespace evaluation = cetp::evaluation;
namespace rooms = cetp::rooms;
namespace slots = cetp::slots;
namespace aco = slots::aco;
namespace local_search = slots::local_search;

void bind_models(nb::module_& module) {
  nb::class_<cetp::ExamSection>(module, "ExamSection")
      .def(
          nb::init<std::string, Vector<std::string>, Vector<std::string>>(),
          nb::arg("code"),
          nb::arg("students"),
          nb::arg("feasible_rooms")
      )
      .def_ro("code", &cetp::ExamSection::code)
      .def_ro("students", &cetp::ExamSection::students)
      .def_ro("feasible_rooms", &cetp::ExamSection::feasible_rooms);

  nb::class_<cetp::Exam>(module, "Exam")
      .def(
          nb::init<std::string, int, Vector<cetp::ExamSection>, Vector<int64_t>>(),
          nb::arg("code"),
          nb::arg("credits"),
          nb::arg("sections"),
          nb::arg("feasible_timestamps")
      )
      .def_ro("code", &cetp::Exam::code)
      .def_ro("credits", &cetp::Exam::credits)
      .def_ro("sections", &cetp::Exam::sections)
      .def_ro("feasible_timestamps", &cetp::Exam::feasible_timestamps);

  nb::class_<cetp::Room>(module, "Room")
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

  nb::class_<cetp::ProblemData>(module, "ProblemData")
      .def(
          nb::init<const Vector<cetp::Exam>&, const Vector<cetp::Room>&>(),
          nb::arg("exams"),
          nb::arg("rooms")
      )
      .def_ro("num_exams", &cetp::ProblemData::num_exams)
      .def_ro("num_slots", &cetp::ProblemData::num_slots)
      .def_ro("num_rooms", &cetp::ProblemData::num_rooms)
      .def_ro("num_sections", &cetp::ProblemData::num_sections)
      .def_ro("num_students", &cetp::ProblemData::num_students)
      .def_ro("num_enrollments", &cetp::ProblemData::num_enrollments)
      .def_ro("exam_codes", &cetp::ProblemData::exam_codes)
      .def_ro("section_codes", &cetp::ProblemData::section_codes)
      .def_ro("room_codes", &cetp::ProblemData::room_codes)
      .def_ro("slot_timestamps", &cetp::ProblemData::slot_timestamps);
}

void bind_params(nb::module_& module) {
  nb::class_<evaluation::Params>(module, "EvalParams")
      .def(nb::init<>())
      .def_prop_rw(
          "penalty_decay_base",
          &evaluation::Params::penalty_decay_base,
          &evaluation::Params::set_penalty_decay_base
      );

  nb::class_<aco::Params>(module, "AcoParams")
      .def(nb::init<>())
      .def_prop_rw("num_iters", &aco::Params::num_iters, &aco::Params::set_num_iters)
      .def_prop_rw("num_ants", &aco::Params::num_ants, &aco::Params::set_num_ants)
      .def_prop_rw("alpha", &aco::Params::alpha, &aco::Params::set_alpha)
      .def_prop_rw("beta", &aco::Params::beta, &aco::Params::set_beta)
      .def_prop_rw("rho", &aco::Params::rho, &aco::Params::set_rho)
      .def_prop_rw("tau_min", &aco::Params::tau_min, &aco::Params::set_tau_min)
      .def_prop_rw("tau_max", &aco::Params::tau_max, &aco::Params::set_tau_max)
      .def_prop_rw("max_retries", &aco::Params::max_retries, &aco::Params::set_max_retries)
      .def("set_tau_bounds", &aco::Params::set_tau_bounds, nb::arg("tau_min"), nb::arg("tau_max"));

  nb::class_<local_search::Params>(module, "LsParams")
      .def(nb::init<>())
      .def_prop_rw(
          "prob_1_move",
          &local_search::Params::prob_1_move,
          &local_search::Params::set_prob_1_move
      )
      .def_prop_rw(
          "max_improvements",
          &local_search::Params::max_improvements,
          &local_search::Params::set_max_improvements
      )
      .def_prop_rw(
          "patience",
          &local_search::Params::patience,
          &local_search::Params::set_patience
      );

  nb::class_<rooms::Params>(module, "RoomAssignmentParams")
      .def(nb::init<>())
      .def_prop_rw(
          "max_time_seconds",
          &rooms::Params::max_time_seconds,
          &rooms::Params::set_max_time_seconds
      )
      .def_prop_rw("num_workers", &rooms::Params::num_workers, &rooms::Params::set_num_workers)
      .def_prop_rw("random_seed", &rooms::Params::random_seed, &rooms::Params::set_random_seed);

  nb::class_<cetp::Hyperparams>(module, "Hyperparams")
      .def(
          nb::init<evaluation::Params, aco::Params, local_search::Params, rooms::Params>(),
          nb::arg("eval") = evaluation::Params(),
          nb::arg("aco") = aco::Params(),
          nb::arg("ls") = local_search::Params(),
          nb::arg("room_assignment") = rooms::Params()
      )
      .def_rw("eval", &cetp::Hyperparams::eval)
      .def_rw("aco", &cetp::Hyperparams::aco)
      .def_rw("ls", &cetp::Hyperparams::ls)
      .def_rw("room_assignment", &cetp::Hyperparams::room_assignment);
}

void bind_results(nb::module_& module) {
  nb::class_<slots::SlotResult>(module, "SlotResult")
      .def_prop_ro("found", &slots::SlotResult::found)
      .def_ro("schedule", &slots::SlotResult::schedule)
      .def_ro("penalty", &slots::SlotResult::penalty);

  nb::enum_<rooms::RoomAssignmentStatus>(module, "RoomAssignmentStatus")
      .value("OPTIMAL", rooms::RoomAssignmentStatus::OPTIMAL)
      .value("FEASIBLE", rooms::RoomAssignmentStatus::FEASIBLE)
      .value("INFEASIBLE", rooms::RoomAssignmentStatus::INFEASIBLE)
      .value("UNKNOWN", rooms::RoomAssignmentStatus::UNKNOWN)
      .value("MODEL_INVALID", rooms::RoomAssignmentStatus::MODEL_INVALID);

  nb::class_<rooms::RoomAssignment>(module, "RoomAssignment")
      .def_ro("status", &rooms::RoomAssignment::status)
      .def_ro("slot_statuses", &rooms::RoomAssignment::slot_statuses)
      .def_prop_ro("found", &rooms::RoomAssignment::found)
      .def_prop_ro("room_count", &rooms::RoomAssignment::room_count)
      .def_ro("extra_locations", &rooms::RoomAssignment::extra_locations)
      .def_ro("unused_capacity", &rooms::RoomAssignment::unused_capacity)
      .def(
          "rooms_for_section",
          [](const rooms::RoomAssignment& assignment, int section) -> Vector<int> {
            const Span<const int> section_rooms = assignment.rooms_for_section(section);
            return {section_rooms.begin(), section_rooms.end()};
          },
          nb::arg("section_id")
      );

  nb::class_<cetp::TimetableResult>(module, "TimetableResult")
      .def_ro("slots", &cetp::TimetableResult::slots)
      .def_ro("rooms", &cetp::TimetableResult::rooms);
}

void bind_solvers(nb::module_& module) {
  module.def(
      "solve_slots_aco",
      &aco::solve,
      nb::arg("problem"),
      nb::arg("aco_params"),
      nb::arg("eval_params"),
      nb::kw_only(),
      nb::arg("base_seed") = DEFAULT_BASE_SEED,
      nb::arg("callback") = nb::none(),
      nb::call_guard<nb::gil_scoped_release>()
  );

  module.def(
      "improve_slots",
      &local_search::improve,
      nb::arg("problem"),
      nb::arg("schedule"),
      nb::arg("ls_params"),
      nb::arg("eval_params"),
      nb::kw_only(),
      nb::arg("base_seed") = DEFAULT_BASE_SEED,
      nb::call_guard<nb::gil_scoped_release>()
  );

  module.def(
      "assign_rooms",
      &rooms::assign_rooms,
      nb::arg("problem"),
      nb::arg("schedule"),
      nb::arg("room_params"),
      nb::call_guard<nb::gil_scoped_release>()
  );

  module.def(
      "solve_timetable",
      &cetp::solve_timetable,
      nb::arg("problem"),
      nb::arg("hyperparams"),
      nb::kw_only(),
      nb::arg("base_seed") = DEFAULT_BASE_SEED,
      nb::arg("callback") = nb::none(),
      nb::call_guard<nb::gil_scoped_release>()
  );
}

}  // namespace

// NOLINTNEXTLINE(performance-unnecessary-value-param)
NB_MODULE(cetp_solvers, module) {
  module.doc() =
      "Some metaheuristic approaches for the Capacitated Examination Timetabling Problem (CETP).";
  bind_models(module);
  bind_params(module);
  bind_results(module);
  bind_solvers(module);
}
