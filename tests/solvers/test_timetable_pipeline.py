"""End-to-end solve_timetable pipeline and cross-stage integration contracts."""

from math import inf

import pytest

import solvers

from tests.solvers.helpers import (
    DAY,
    TIMESTAMPS,
    Status,
    assert_allocation,
    assert_empty_assignment,
    assert_valid_schedule,
    compute_exact_penalty,
    make_exam,
    make_problem,
    make_rooms,
)


@pytest.mark.parametrize("whole_timetable", [False, True])
def test_slot_search_reports_not_found_and_progress_without_a_schedule(whole_timetable):
    sec1 = solvers.ExamSection("MIL001_1", ["24000001", "24000002"], ["EDGE-R001"])
    sec2 = solvers.ExamSection("PE1002_1", ["24000002", "24000003"], ["EDGE-R001"])
    exams = [
        solvers.Exam("MIL001", 1, [sec1], [0]),
        solvers.Exam("PE1002", 1, [sec2], [0]),
    ]
    problem = solvers.ProblemData(exams, [solvers.Room("EDGE-R001", 30)])
    params = solvers.Hyperparams()
    params.aco.num_iters = 2
    params.aco.num_ants = 2
    progress = []

    def callback(iteration, penalty):
        progress.append((iteration, penalty))

    if whole_timetable:
        timetable = solvers.solve_timetable(problem, params, callback=callback)
        assert timetable.rooms is None
        result = timetable.slots
    else:
        result = solvers.solve_slots_aco(problem, params.aco, params.eval, callback=callback)

    assert progress == [(1, inf), (2, inf)]
    assert not result.found
    assert result.schedule == []
    assert result.penalty == inf


def test_pipeline_callback_exception_propagates_and_problem_stays_reusable():
    exam = solvers.Exam("E0", 3, [solvers.ExamSection("S0", ["a"], ["R0"])], [0])
    problem = solvers.ProblemData([exam], [solvers.Room("R0", 10)])
    params = solvers.Hyperparams()
    params.aco.num_ants = 2
    params.aco.num_iters = 2

    def failing_callback(iteration, penalty):
        raise RuntimeError("pipeline callback failed")

    with pytest.raises(RuntimeError, match="pipeline callback failed"):
        solvers.solve_timetable(problem, params, base_seed=42, callback=failing_callback)

    recovered = solvers.solve_timetable(problem, params, base_seed=42)
    assert recovered.slots.found
    assert recovered.slots.schedule == [0]
    assert recovered.rooms is not None
    assert recovered.rooms.status == Status.OPTIMAL


def test_pipeline_determinism_with_same_seed(large_problem_data):
    exams, _, rooms, hp = large_problem_data
    hp.aco.num_ants = 10
    hp.aco.num_iters = 2
    hp.ls.max_improvements = 20
    hp.ls.patience = 50
    problem = solvers.ProblemData(exams, rooms)

    first = solvers.solve_timetable(problem, hp, base_seed=321)
    second = solvers.solve_timetable(problem, hp, base_seed=321)

    assert first.slots.found and second.slots.found
    assert first.slots.schedule == second.slots.schedule
    assert first.slots.penalty == second.slots.penalty
    assert first.rooms.status == second.rooms.status
    assert (first.rooms.room_count, first.rooms.unused_capacity) == (
        second.rooms.room_count,
        second.rooms.unused_capacity,
    )


def test_standalone_slot_algorithms_match_full_pipeline():
    exams = [make_exam(0, 5, ["R0"]), make_exam(1, 5, ["R1"])]
    capacities = [10, 10]
    params = solvers.Hyperparams()
    params.aco.num_ants = 2
    params.aco.num_iters = 1
    problem = make_problem(exams, capacities)

    slots = solvers.solve_slots_aco(problem, params.aco, params.eval)
    assert slots.found
    improved = solvers.improve_slots(problem, slots.schedule, params.ls, params.eval)
    assert improved.found

    for result in (slots, improved):
        assignment = solvers.assign_rooms(problem, result.schedule, params.room_assignment)
        assert assignment.status == Status.OPTIMAL
        assert_allocation(assignment, result.schedule, exams, capacities)

    timetable = solvers.solve_timetable(problem, params)
    assert timetable.slots.schedule == improved.schedule
    assert timetable.slots.penalty == improved.penalty


def test_pipeline_uses_an_immutable_problem_and_returns_independent_results():
    exams = [make_exam(0, 5, ["R0"]), make_exam(1, 5, ["R1"])]
    rooms = make_rooms([10, 10])
    params = solvers.Hyperparams()
    params.aco.num_ants = 2
    params.aco.num_iters = 1
    problem = solvers.ProblemData(exams, rooms)

    # ProblemData holds an immutable snapshot independent of the source objects.
    rooms[0].capacity = 1
    rooms.clear()
    exams.clear()
    timetable = solvers.solve_timetable(problem, params)
    slots, assignment = timetable.slots, timetable.rooms
    del timetable, problem, exams, rooms

    assert slots.found
    schedule = slots.schedule
    assert len(schedule) == 2
    schedule.clear()
    assert len(slots.schedule) == 2
    assert assignment.status == Status.OPTIMAL
    assert assignment.rooms_for_section(0) == [0]
    assert assignment.rooms_for_section(1) == [1]
    assert (assignment.room_count, assignment.unused_capacity) == (2, 10)


def test_pipeline_snapshots_hyperparameters_for_every_stage():
    exams = [
        solvers.Exam("E0", 3, [solvers.ExamSection("S0", ["shared"], ["R0"])], [TIMESTAMPS[0]]),
        solvers.Exam("E1", 3, [solvers.ExamSection("S1", ["shared"], ["R0"])], [TIMESTAMPS[1]]),
    ]
    params = solvers.Hyperparams()
    params.aco.num_ants = 2
    params.aco.num_iters = 1
    params.eval.penalty_decay_base = 8.0
    problem = make_problem(exams, [10])
    progress = []

    def callback(iteration, penalty):
        progress.append((iteration, penalty))
        params.aco.num_iters = 2
        params.eval.penalty_decay_base = 2.0
        params.room_assignment.slot_time_limit = 1e-300

    timetable = solvers.solve_timetable(problem, params, callback=callback)
    assert timetable.slots.schedule == [0, 1]
    # One shared student, 3 + 3 credits, eight hours apart at decay base 8.
    assert timetable.slots.penalty == pytest.approx(3.0)
    assert progress == [(1, timetable.slots.penalty)]
    assert timetable.rooms.status == Status.OPTIMAL

    repeated = solvers.solve_timetable(problem, params)
    assert repeated.slots.schedule == timetable.slots.schedule
    assert repeated.slots.penalty == pytest.approx(6.0 * 2.0 ** (-1.0 / 3.0))
    assert_empty_assignment(repeated.rooms, Status.UNKNOWN)


def test_pipeline_reports_room_infeasibility_without_repairing_the_final_slots():
    exams = [
        make_exam(0, 5, ["R0"], [TIMESTAMPS[0]]),
        make_exam(1, 5, ["R0"], [TIMESTAMPS[0]]),
    ]
    rooms = make_rooms([10])
    params = solvers.Hyperparams()
    params.aco.num_ants = 2
    params.aco.num_iters = 1
    problem = solvers.ProblemData(exams, rooms)
    timetable = solvers.solve_timetable(problem, params)

    # The two sections cannot share their only room, even though it has ten seats.
    assert timetable.slots.schedule == [0, 0]
    assert timetable.slots.penalty == 0.0
    assert_empty_assignment(timetable.rooms, Status.INFEASIBLE)


@pytest.mark.parametrize("probability", [0.0, 1.0])
def test_valid_parameter_boundaries_work_through_the_pipeline(probability):
    params = solvers.Hyperparams()
    params.aco.num_ants = 2
    params.aco.num_iters = 1
    params.aco.alpha = 0.0
    params.aco.beta = 0.0
    params.aco.rho = probability
    params.aco.set_tau_bounds(2.0, 3.0)
    params.ls.prob_1_move = probability
    params.ls.max_improvements = 0
    params.ls.patience = 0
    exam = solvers.Exam("E", 3, [solvers.ExamSection("S", ["student"], ["R0"])], [0])
    problem = solvers.ProblemData([exam], [solvers.Room("R0", 10)])
    timetable = solvers.solve_timetable(problem, params)
    assert timetable.slots.schedule == [0]
    assert timetable.rooms is not None
    assert timetable.rooms.status == Status.OPTIMAL
    assert timetable.rooms.rooms_for_section(0) == [0]


def test_multi_section_exam_conflicts_and_sorted_section_room_mapping():
    # Pass E0's sections in reverse code order; Exam sorts them to [E0_S1, E0_S2].
    sec_big = solvers.ExamSection("E0_S2", [f"big_{i}" for i in range(30)], ["R_BIG"])
    sec_small = solvers.ExamSection("E0_S1", ["shared"] + [f"s_{i}" for i in range(9)], ["R_SMALL"])
    e0 = solvers.Exam("E0", 3, [sec_big, sec_small], [0, DAY])
    e1 = solvers.Exam("E1", 3, [solvers.ExamSection("E1_S1", ["shared"], ["R_SMALL"])], [0, DAY])

    rooms = [solvers.Room("R_BIG", 30), solvers.Room("R_SMALL", 10)]
    problem = solvers.ProblemData([e0, e1], rooms)
    assert problem.section_codes == ["E0_S1", "E0_S2", "E1_S1"]
    assert problem.room_codes == ["R_BIG", "R_SMALL"]

    params = solvers.Hyperparams()
    params.aco.num_ants = 4
    params.aco.num_iters = 2
    timetable = solvers.solve_timetable(problem, params, base_seed=7)

    assert timetable.slots.found
    assert_valid_schedule(problem, [e0, e1], timetable.slots.schedule)
    assert timetable.slots.penalty == pytest.approx(
        compute_exact_penalty(
            problem, [e0, e1], timetable.slots.schedule, params.eval.penalty_decay_base
        )
    )
    assert timetable.rooms is not None
    assert timetable.rooms.status == Status.OPTIMAL
    assert [problem.room_codes[r] for r in timetable.rooms.rooms_for_section(0)] == ["R_SMALL"]
    assert [problem.room_codes[r] for r in timetable.rooms.rooms_for_section(1)] == ["R_BIG"]
    assert [problem.room_codes[r] for r in timetable.rooms.rooms_for_section(2)] == ["R_SMALL"]


def test_sparse_and_negative_timestamps_solve_optimally_through_pipeline():
    e0 = solvers.Exam("E0", 2, [solvers.ExamSection("S0", ["shared"], ["R0"])], [2 * DAY, -DAY])
    e1 = solvers.Exam("E1", 4, [solvers.ExamSection("S1", ["shared"], ["R0"])], [2 * DAY, 0])
    problem = solvers.ProblemData([e0, e1], [solvers.Room("R0", 10)])

    params = solvers.Hyperparams()
    params.aco.num_ants = 10
    params.aco.num_iters = 5
    params.ls.max_improvements = 50
    params.ls.patience = 100
    timetable = solvers.solve_timetable(problem, params, base_seed=19)

    assert timetable.slots.found
    assert_valid_schedule(problem, [e0, e1], timetable.slots.schedule)
    assert timetable.slots.penalty == pytest.approx(
        compute_exact_penalty(
            problem, [e0, e1], timetable.slots.schedule, params.eval.penalty_decay_base
        )
    )
    # Optimal separation is 3 days: E0 at -DAY (slot 0) and E1 at +2*DAY (slot 2).
    assert timetable.slots.schedule == [0, 2]


def test_validate_schedule_accepts_valid_and_rejects_invalid_schedules():
    e0 = solvers.Exam("E0", 3, [solvers.ExamSection("S0", ["shared"], ["R0"])], [0, DAY])
    e1 = solvers.Exam("E1", 3, [solvers.ExamSection("S1", ["shared"], ["R0"])], [0])
    problem = solvers.ProblemData([e0, e1], [solvers.Room("R0", 10)])

    solvers.validate_schedule(problem, [1, 0])

    with pytest.raises(ValueError, match="Schedule length"):
        solvers.validate_schedule(problem, [1])
    with pytest.raises(ValueError, match="infeasible slot"):
        solvers.validate_schedule(problem, [0, 1])
    with pytest.raises(ValueError, match="conflict"):
        solvers.validate_schedule(problem, [0, 0])


def test_complete_timetable_handles_found_and_not_found_slot_results():
    e0 = solvers.Exam("E0", 3, [solvers.ExamSection("S0", ["shared"], ["R0"])], [0])
    e1 = solvers.Exam("E1", 3, [solvers.ExamSection("S1", ["shared"], ["R0"])], [0])
    infeasible_problem = solvers.ProblemData([e0, e1], [solvers.Room("R0", 10)])
    params = solvers.Hyperparams()
    params.aco.num_ants = 2
    params.aco.num_iters = 1

    unfound_slots = solvers.solve_slots_aco(infeasible_problem, params.aco, params.eval)
    unfound_tt = solvers.complete_timetable(
        infeasible_problem, unfound_slots, params.room_assignment
    )
    assert not unfound_tt.slots.found
    assert unfound_tt.rooms is None

    feasible_problem = solvers.ProblemData([e0], [solvers.Room("R0", 10)])
    found_slots = solvers.solve_slots_aco(feasible_problem, params.aco, params.eval, base_seed=77)
    found_tt = solvers.complete_timetable(
        feasible_problem, found_slots, params.room_assignment, base_seed=77
    )
    assert found_tt.slots.found
    assert found_tt.slots.schedule == [0]
    assert found_tt.rooms is not None
    assert found_tt.rooms.status == Status.OPTIMAL
    assert found_tt.rooms.rooms_for_section(0) == [0]


def test_solve_timetable_propagates_unified_base_seed_to_room_assignment():
    # Give enough rooms so every valid schedule is room-feasible with symmetric room choices,
    # ensuring solve_timetable and the manual 3-stage pipeline produce identical room allocations.
    rooms = [solvers.Room(f"R{i:02d}", 40) for i in range(12)]
    room_codes = [room.code for room in rooms]
    exams = [
        solvers.Exam(
            f"E{i}",
            3,
            [solvers.ExamSection(f"E{i}_S0", [f"S{i}_{k}" for k in range(20)], room_codes)],
            TIMESTAMPS,
        )
        for i in range(8)
    ]
    problem = solvers.ProblemData(exams, rooms)
    hp = solvers.Hyperparams()
    hp.aco.num_ants = 6
    hp.aco.num_iters = 2
    hp.ls.max_improvements = 10
    hp.ls.patience = 20

    seed = 2026
    full = solvers.solve_timetable(problem, hp, base_seed=seed)
    slots = solvers.solve_slots_aco(problem, hp.aco, hp.eval, base_seed=seed)
    improved = solvers.improve_slots(problem, slots.schedule, hp.ls, hp.eval, base_seed=seed)
    completed = solvers.complete_timetable(problem, improved, hp.room_assignment, base_seed=seed)

    assert full.slots.found and completed.slots.found
    assert full.slots.schedule == completed.slots.schedule
    assert full.rooms is not None and completed.rooms is not None
    assert full.rooms.found and completed.rooms.found
    assert full.rooms.status == completed.rooms.status
    assert [full.rooms.rooms_for_section(i) for i in range(problem.num_sections)] == [
        completed.rooms.rooms_for_section(i) for i in range(problem.num_sections)
    ]


def test_complete_timetable_preserves_found_slots_when_rooms_fail_or_time_out():
    e0 = solvers.Exam("E0", 3, [solvers.ExamSection("S0", [f"a{i}" for i in range(15)], ["R0"])], [0])
    problem = solvers.ProblemData([e0], [solvers.Room("R0", 10)])
    params = solvers.Hyperparams()
    params.aco.num_ants = 2
    params.aco.num_iters = 1

    slots = solvers.solve_slots_aco(problem, params.aco, params.eval, base_seed=42)
    assert slots.found and slots.schedule == [0]

    infeasible_tt = solvers.complete_timetable(problem, slots, params.room_assignment)
    assert infeasible_tt.slots.found and infeasible_tt.slots.schedule == [0]
    assert infeasible_tt.rooms is not None
    assert_empty_assignment(infeasible_tt.rooms, Status.INFEASIBLE)

    ok_problem = solvers.ProblemData([e0], [solvers.Room("R0", 20)])
    ok_slots = solvers.solve_slots_aco(ok_problem, params.aco, params.eval, base_seed=42)
    tiny_rooms = solvers.RoomAssignmentParams()
    tiny_rooms.slot_time_limit = 1e-300
    timeout_tt = solvers.complete_timetable(ok_problem, ok_slots, tiny_rooms)
    assert timeout_tt.slots.found and timeout_tt.slots.schedule == [0]
    assert timeout_tt.rooms is not None
    assert_empty_assignment(timeout_tt.rooms, Status.UNKNOWN)


@pytest.mark.parametrize("base_seed", [0, 2**64 - 1])
def test_solve_timetable_is_deterministic_at_64bit_seed_boundaries(base_seed):
    rooms = [solvers.Room(f"R{i:02d}", 30, "TTT" if i < 2 else "KM") for i in range(4)]
    codes = [room.code for room in rooms]
    exams = [
        solvers.Exam(
            f"E{i}",
            2 + (i % 3),
            [solvers.ExamSection(f"E{i}_S0", [f"u_{i}_{k}" for k in range(10)] + ["shared"], codes)],
            [0, DAY, 2 * DAY, 3 * DAY],
        )
        for i in range(4)
    ]
    problem = solvers.ProblemData(exams, rooms)
    hp = solvers.Hyperparams()
    hp.aco.num_ants = 6
    hp.aco.num_iters = 3
    hp.ls.max_improvements = 20
    hp.ls.patience = 40

    first = solvers.solve_timetable(problem, hp, base_seed=base_seed)
    second = solvers.solve_timetable(problem, hp, base_seed=base_seed)

    assert first.slots.found and second.slots.found
    assert first.slots.schedule == second.slots.schedule
    assert first.slots.penalty == second.slots.penalty
    assert_valid_schedule(problem, exams, first.slots.schedule)
    assert first.rooms is not None and second.rooms is not None
    assert first.rooms.status == Status.OPTIMAL
    assert [first.rooms.rooms_for_section(i) for i in range(4)] == [
        second.rooms.rooms_for_section(i) for i in range(4)
    ]
    assert_allocation(
        first.rooms,
        first.slots.schedule,
        exams,
        [r.capacity for r in rooms],
        [r.location for r in rooms],
        codes,
    )
