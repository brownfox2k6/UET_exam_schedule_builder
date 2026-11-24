"""Room allocation contracts, checked against exhaustive search on small problems."""

from itertools import combinations, product
import random

import pytest

import solvers

from tests.solvers.helpers import (
    TIMESTAMPS,
    Status,
    assert_allocation,
    assert_empty_assignment,
    assign_rooms_for,
    make_exam,
    make_problem,
    room_code,
)


def test_multiple_sections_split_rooms_and_reuse_room_zero_in_another_slot():
    sections = [
        solvers.ExamSection("A", [f"A{i}" for i in range(65)], ["R0", "R1"]),
        solvers.ExamSection("B", [f"B{i}" for i in range(20)], ["R1", "R2"]),
    ]
    exams = [
        solvers.Exam("E", 3, sections, [TIMESTAMPS[0]]),
        make_exam(1, 35, ["R0"], [TIMESTAMPS[1]]),
    ]
    capacities = [40, 30, 20]
    schedule = [0, 1]
    assignment = assign_rooms_for(exams, capacities, schedule)

    assert assignment.status == Status.OPTIMAL
    assert assignment.rooms_for_section(0) == [0, 1]
    assert assignment.rooms_for_section(1) == [2]
    assert assignment.rooms_for_section(2) == [0]
    assert_allocation(assignment, schedule, exams, capacities)


def test_single_large_section_splits_across_three_rooms_optimally():
    exams = [make_exam(0, 100, ["R0", "R1", "R2", "R3"])]
    capacities = [40, 40, 25, 20]
    params = solvers.RoomAssignmentParams()
    params.num_workers = 2
    params.random_seed = 0
    assignment = solvers.assign_rooms(make_problem(exams, capacities), [0], params)

    assert assignment.status == Status.OPTIMAL
    assert assignment.rooms_for_section(0) == [0, 1, 3]
    assert (assignment.room_count, assignment.unused_capacity) == (3, 0)
    assert_allocation(assignment, [0], exams, capacities)


def test_room_count_has_priority_over_unused_seats():
    exams = [make_exam(0, 60, ["R0", "R1", "R2"])]
    assignment = assign_rooms_for(exams, [100, 30, 30], [0])
    assert assignment.status == Status.OPTIMAL
    assert assignment.rooms_for_section(0) == [0]
    assert (assignment.room_count, assignment.unused_capacity) == (1, 40)


def test_unused_seats_break_ties_between_allocations_with_equal_room_count():
    exams = [make_exam(0, 60, ["R0", "R1", "R2"])]
    assignment = assign_rooms_for(exams, [100, 70, 60], [0])
    assert assignment.status == Status.OPTIMAL
    assert assignment.rooms_for_section(0) == [2]
    assert (assignment.room_count, assignment.unused_capacity) == (1, 0)


def test_global_assignment_avoids_a_greedy_room_choice():
    exams = [make_exam(0, 60, ["R0", "R1"]), make_exam(1, 100, ["R0"])]
    assignment = assign_rooms_for(exams, [100, 60], [0, 0])
    assert assignment.status == Status.OPTIMAL
    assert assignment.rooms_for_section(0) == [1]
    assert assignment.rooms_for_section(1) == [0]
    assert_allocation(assignment, [0, 0], exams, [100, 60])


def test_room_codes_map_independently_of_alphabetical_room_order():
    rooms = [solvers.Room("R_LARGE", 100), solvers.Room("R_EXACT", 30)]
    section = solvers.ExamSection(
        "S0", [f"SV{i}" for i in range(30)], ["R_LARGE", "R_EXACT"]
    )
    exam = solvers.Exam("E0", 3, [section], [TIMESTAMPS[0]])
    problem = solvers.ProblemData([exam], rooms)
    assignment = solvers.assign_rooms(problem, [0], solvers.RoomAssignmentParams())

    assert problem.room_codes == ["R_LARGE", "R_EXACT"]
    assert assignment.status == Status.OPTIMAL
    assert [problem.room_codes[r] for r in assignment.rooms_for_section(0)] == [
        "R_EXACT"
    ]
    assert (assignment.room_count, assignment.unused_capacity) == (1, 0)


@pytest.mark.parametrize(
    "counts,eligible,capacities",
    [
        (
            [11],
            [["R0", "R1"]],
            [5, 5],
        ),  # Insufficient eligible capacity for one section.
        (
            [5, 5],
            [["R0"], ["R0"]],
            [10],
        ),  # Spare seats cannot be shared between sections.
        (
            [5, 5, 5],
            [["R0"], ["R0"], ["R1", "R2"]],
            [10, 10, 10],
        ),  # Eligibility bottleneck.
    ],
)
def test_infeasible_allocations_never_leave_partial_results(
    counts, eligible, capacities
):
    exams = [
        make_exam(i, count, rooms)
        for i, (count, rooms) in enumerate(zip(counts, eligible))
    ]
    assignment = assign_rooms_for(exams, capacities, [0] * len(exams))
    assert_empty_assignment(assignment, len(exams), Status.INFEASIBLE)


def test_timeout_result_is_independent_of_other_room_assignments():
    exams = [make_exam(0, 5, ["R0", "R1"])]
    problem = make_problem(exams, [8, 5])
    params = solvers.RoomAssignmentParams()
    first = solvers.assign_rooms(problem, [0], params)
    assert first.status == Status.OPTIMAL

    params.max_time_seconds = (
        1e-300  # Deterministically expires before the solver is called.
    )
    assert_empty_assignment(
        solvers.assign_rooms(problem, [0], params), 1, Status.UNKNOWN
    )
    assert first.rooms_for_section(0) == [1]

    retried = solvers.assign_rooms(problem, [0], solvers.RoomAssignmentParams())
    assert retried.status == Status.OPTIMAL
    assert retried.rooms_for_section(0) == [1]


def test_reallocation_with_reduced_capacity_reports_infeasible():
    exams = [make_exam(0, 10, ["R0"])]
    assignment = assign_rooms_for(exams, [10], [0])
    assert assignment.status == Status.OPTIMAL
    assert_empty_assignment(assign_rooms_for(exams, [9], [0]), 1, Status.INFEASIBLE)
    assert assignment.rooms_for_section(0) == [0]


def test_room_allocation_preserves_a_nonzero_timetable_penalty():
    exams = [
        solvers.Exam(
            f"E{i}", 3, [solvers.ExamSection(f"S{i}", ["shared"], ["R0"])], TIMESTAMPS
        )
        for i in range(2)
    ]
    problem = make_problem(exams, [10])
    params = solvers.Hyperparams()
    params.ls.max_improvements = 0
    slots = solvers.improve_slots(problem, [0, 1], params.ls, params.eval)
    original_penalty = slots.penalty
    assert original_penalty > 0

    assignment = solvers.assign_rooms(problem, slots.schedule, params.room_assignment)
    assert assignment.status == Status.OPTIMAL
    assert slots.schedule == [0, 1]
    assert slots.penalty == original_penalty
    assert_allocation(assignment, slots.schedule, exams, [10])


def test_summary_uses_64_bit_capacity_totals():
    capacities = [2**31 - 1, 2**31 - 2]
    exams = [make_exam(0, 1, ["R0"]), make_exam(1, 1, ["R1"])]
    assignment = assign_rooms_for(exams, capacities, [0, 0])
    assert assignment.status == Status.OPTIMAL
    assert assignment.unused_capacity == sum(capacities) - 2
    assert_allocation(assignment, [0, 0], exams, capacities)


def test_room_results_are_copies_and_outlive_the_problem():
    problem = make_problem([make_exam(0, 5, ["R0"])], [10])
    assignment = solvers.assign_rooms(problem, [0], solvers.RoomAssignmentParams())
    assert assignment.status == Status.OPTIMAL

    rooms = assignment.rooms_for_section(0)
    rooms.clear()
    del problem
    assert assignment.rooms_for_section(0) == [0]
    with pytest.raises(IndexError):
        assignment.rooms_for_section(-1)
    with pytest.raises(IndexError):
        assignment.rooms_for_section(1)


def test_assign_rooms_accepts_different_external_schedules_for_the_same_problem():
    exams = [make_exam(0, 5, ["R0"]), make_exam(1, 5, ["R0"])]
    params = solvers.RoomAssignmentParams()
    problem = make_problem(exams, [10])

    first = solvers.assign_rooms(problem, [0, 1], params)
    assert first.status == Status.OPTIMAL
    second = solvers.assign_rooms(problem, [0, 0], params)
    assert_empty_assignment(second, 2, Status.INFEASIBLE)
    assert (first.room_count, first.unused_capacity) == (2, 10)
    assert first.rooms_for_section(0) == [0]
    assert first.rooms_for_section(1) == [0]

    third = solvers.assign_rooms(problem, [1, 0], params)
    assert third.status == Status.OPTIMAL
    assert_allocation(third, [1, 0], exams, [10])


def exhaustive_optimum(counts, eligible, capacities, schedule):
    alternatives = []
    for count, rooms in zip(counts, eligible):
        alternatives.append(
            [
                choice
                for size in range(1, len(rooms) + 1)
                for choice in combinations(rooms, size)
                if sum(capacities[room] for room in choice) >= count
            ]
        )
    best = None
    for assignment in product(*alternatives):
        uses = [
            (slot, room) for slot, rooms in zip(schedule, assignment) for room in rooms
        ]
        if len(uses) != len(set(uses)):
            continue
        objective = (len(uses), sum(capacities[room] for _, room in uses) - sum(counts))
        best = objective if best is None else min(best, objective)
    return best


@pytest.mark.parametrize("seed", range(20))
def test_matches_exhaustive_lexicographic_optimum(seed):
    rng = random.Random(seed)
    capacities = [rng.randint(2, 8) for _ in range(3)]
    counts = [rng.randint(1, 9) for _ in range(4)]
    eligible = [sorted(rng.sample(range(3), rng.randint(1, 3))) for _ in counts]
    schedule = [rng.randrange(2) for _ in counts]
    expected = exhaustive_optimum(counts, eligible, capacities, schedule)
    exams = [
        make_exam(i, count, [room_code(room) for room in rooms])
        for i, (count, rooms) in enumerate(zip(counts, eligible))
    ]
    assignment = assign_rooms_for(exams, capacities, schedule)
    if expected is None:
        assert_empty_assignment(assignment, len(exams), Status.INFEASIBLE)
    else:
        assert assignment.status == Status.OPTIMAL
        assert (assignment.room_count, assignment.unused_capacity) == expected
        assert_allocation(assignment, schedule, exams, capacities)


def test_section_cannot_split_rooms_across_different_locations():
    # 65 students cannot combine a 40-seat room at TTT with a 40-seat room at KM.
    rooms = [
        solvers.Room("101-T", 40, "Ton That Thuyet"),
        solvers.Room("103-A", 40, "Kieu Mai"),
    ]
    section = solvers.ExamSection("S0", [f"SV{i}" for i in range(65)], ["101-T", "103-A"])
    exam = solvers.Exam("E0", 3, [section], [TIMESTAMPS[0]])
    problem = solvers.ProblemData([exam], rooms)

    assignment = solvers.assign_rooms(problem, [0], solvers.RoomAssignmentParams())
    assert_empty_assignment(assignment, 1, Status.INFEASIBLE)
    assert assignment.slot_statuses == [Status.INFEASIBLE]


def test_section_picks_same_location_rooms_when_splitting():
    rooms = [
        solvers.Room("101-T", 40, "Ton That Thuyet"),
        solvers.Room("102-T", 30, "Ton That Thuyet"),
        solvers.Room("103-A", 50, "Kieu Mai"),
    ]
    section = solvers.ExamSection("S0", [f"SV{i}" for i in range(65)], ["101-T", "102-T", "103-A"])
    exam = solvers.Exam("E0", 3, [section], [TIMESTAMPS[0]])
    problem = solvers.ProblemData([exam], rooms)

    assignment = solvers.assign_rooms(problem, [0], solvers.RoomAssignmentParams())
    assert assignment.status == Status.OPTIMAL
    assert [problem.room_codes[r] for r in assignment.rooms_for_section(0)] == ["101-T", "102-T"]
    assert (assignment.room_count, assignment.extra_locations, assignment.unused_capacity) == (
        2,
        0,
        5,
    )


def test_room_count_has_priority_over_exam_location_spread():
    # Keeping both 60-student sections at TTT needs 3 rooms (60 + 30 + 30), while splitting
    # across TTT (60) and KM (60) needs only 2 rooms. Room count must win.
    rooms = [
        solvers.Room("101-T", 60, "Ton That Thuyet"),
        solvers.Room("102-T", 30, "Ton That Thuyet"),
        solvers.Room("103-T", 30, "Ton That Thuyet"),
        solvers.Room("201-A", 60, "Kieu Mai"),
    ]
    all_codes = [room.code for room in rooms]
    sections = [
        solvers.ExamSection("S0", [f"A{i}" for i in range(60)], all_codes),
        solvers.ExamSection("S1", [f"B{i}" for i in range(60)], all_codes),
    ]
    problem = solvers.ProblemData([solvers.Exam("E0", 3, sections, [TIMESTAMPS[0]])], rooms)

    assignment = solvers.assign_rooms(problem, [0], solvers.RoomAssignmentParams())
    assert assignment.status == Status.OPTIMAL
    assert (assignment.room_count, assignment.extra_locations, assignment.unused_capacity) == (
        2,
        1,
        0,
    )


def test_exam_location_consolidation_has_priority_over_unused_seats():
    # Both 30-student sections can stay at TTT (50 + 50 seats -> 40 unused, 0 extra locations)
    # or split to TTT + KM (50 + 30 seats -> 20 unused, 1 extra location). Consolidation must win.
    rooms = [
        solvers.Room("101-T", 50, "Ton That Thuyet"),
        solvers.Room("102-T", 50, "Ton That Thuyet"),
        solvers.Room("201-A", 30, "Kieu Mai"),
    ]
    all_codes = [room.code for room in rooms]
    sections = [
        solvers.ExamSection("S0", [f"A{i}" for i in range(30)], all_codes),
        solvers.ExamSection("S1", [f"B{i}" for i in range(30)], all_codes),
    ]
    problem = solvers.ProblemData([solvers.Exam("E0", 3, sections, [TIMESTAMPS[0]])], rooms)

    assignment = solvers.assign_rooms(problem, [0], solvers.RoomAssignmentParams())
    assert assignment.status == Status.OPTIMAL
    assert (assignment.room_count, assignment.extra_locations, assignment.unused_capacity) == (
        2,
        0,
        40,
    )


def test_flexible_section_joins_fixed_location_sibling_in_same_exam():
    # S0 is fixed to TTT (only eligible for 101-T). S1 can use 102-T (50 seats, TTT) or
    # 201-A (30 seats, KM). S1 should join S0 at TTT to achieve 0 extra locations.
    rooms = [
        solvers.Room("101-T", 30, "Ton That Thuyet"),
        solvers.Room("102-T", 50, "Ton That Thuyet"),
        solvers.Room("201-A", 30, "Kieu Mai"),
    ]
    sections = [
        solvers.ExamSection("S0", [f"A{i}" for i in range(30)], ["101-T"]),
        solvers.ExamSection("S1", [f"B{i}" for i in range(30)], ["102-T", "201-A"]),
    ]
    problem = solvers.ProblemData([solvers.Exam("E0", 3, sections, [TIMESTAMPS[0]])], rooms)

    assignment = solvers.assign_rooms(problem, [0], solvers.RoomAssignmentParams())
    assert assignment.status == Status.OPTIMAL
    assert [problem.room_codes[r] for r in assignment.rooms_for_section(1)] == ["102-T"]
    assert (assignment.room_count, assignment.extra_locations, assignment.unused_capacity) == (
        2,
        0,
        20,
    )


def test_per_slot_diagnostics_report_all_infeasible_slots_and_empty_slots():
    # 4 slots: slot 0 feasible, slot 1 empty, slot 2 infeasible, slot 3 infeasible.
    ts = [1000, 2000, 3000, 4000]
    exams = [
        make_exam(0, 5, ["R0"], ts),
        make_exam(1, 20, ["R0"], ts),  # Too large for R0 (10 seats) -> slot 2 infeasible.
        make_exam(2, 20, ["R0"], ts),  # Too large for R0 (10 seats) -> slot 3 infeasible.
    ]
    problem = make_problem(exams, [10])
    params = solvers.RoomAssignmentParams()
    params.num_workers = 2

    assignment = solvers.assign_rooms(problem, [0, 2, 3], params)
    assert_empty_assignment(assignment, 3, Status.INFEASIBLE)
    assert len(assignment.slot_statuses) == 4
    assert assignment.slot_statuses[0] in (Status.OPTIMAL, Status.FEASIBLE)
    assert assignment.slot_statuses[1] == Status.OPTIMAL
    assert assignment.slot_statuses[2] == Status.INFEASIBLE
    assert assignment.slot_statuses[3] == Status.INFEASIBLE


def exhaustive_3level_optimum(exam_section_specs, room_capacities, room_locations, schedule):
    """Brute-force oracle for (room_count, extra_locations, unused_capacity) with hard section loc."""
    section_alternatives = []
    for exam_id, sections in enumerate(exam_section_specs):
        for count, eligible_rooms in sections:
            choices = []
            for size in range(1, len(eligible_rooms) + 1):
                for combo in combinations(eligible_rooms, size):
                    locs = {room_locations[r] for r in combo}
                    if len(locs) != 1:
                        continue
                    if sum(room_capacities[r] for r in combo) >= count:
                        choices.append((exam_id, count, combo, next(iter(locs))))
            section_alternatives.append(choices)

    best = None
    for assignment in product(*section_alternatives):
        uses = []
        exam_locs = {}
        total_unused = 0
        for exam_id, count, combo, loc in assignment:
            slot = schedule[exam_id]
            for room in combo:
                uses.append((slot, room))
            exam_locs.setdefault(exam_id, set()).add(loc)
            total_unused += sum(room_capacities[r] for r in combo) - count
        if len(uses) != len(set(uses)):
            continue
        extra_locs = sum(len(locs) - 1 for locs in exam_locs.values())
        objective = (len(uses), extra_locs, total_unused)
        best = objective if best is None else min(best, objective)
    return best


@pytest.mark.parametrize("seed", range(20))
def test_matches_exhaustive_3level_location_optimum_and_parallel_determinism(seed):
    rng = random.Random(1000 + seed)
    num_rooms = 4
    capacities = [rng.randint(3, 8) for _ in range(num_rooms)]
    locations = [rng.choice(["TTT", "KM", "XT"]) for _ in range(num_rooms)]
    rooms = [solvers.Room(room_code(i), capacities[i], locations[i]) for i in range(num_rooms)]

    # 2 exams, each with 1 or 2 sections, placed across 2 slots.
    exam_section_specs = []
    exams = []
    for exam_id in range(2):
        num_secs = rng.randint(1, 2)
        sec_specs = []
        sections = []
        for sec_idx in range(num_secs):
            count = rng.randint(1, 9)
            eligible = sorted(rng.sample(range(num_rooms), rng.randint(1, num_rooms)))
            sec_specs.append((count, eligible))
            sections.append(
                solvers.ExamSection(
                    f"E{exam_id}_S{sec_idx}",
                    [f"E{exam_id}_S{sec_idx}_{k}" for k in range(count)],
                    [room_code(r) for r in eligible],
                )
            )
        exam_section_specs.append(sec_specs)
        exams.append(solvers.Exam(f"E{exam_id}", 3, sections, TIMESTAMPS))

    schedule = [rng.randrange(2) for _ in exams]
    expected = exhaustive_3level_optimum(exam_section_specs, capacities, locations, schedule)
    problem = solvers.ProblemData(exams, rooms)

    params_seq = solvers.RoomAssignmentParams()
    params_seq.num_workers = 1
    params_par = solvers.RoomAssignmentParams()
    params_par.num_workers = 2

    res_seq = solvers.assign_rooms(problem, schedule, params_seq)
    res_par = solvers.assign_rooms(problem, schedule, params_par)

    if expected is None:
        assert_empty_assignment(res_seq, problem.num_sections, Status.INFEASIBLE)
        assert_empty_assignment(res_par, problem.num_sections, Status.INFEASIBLE)
        seq_infeasible = [i for i, st in enumerate(res_seq.slot_statuses) if st == Status.INFEASIBLE]
        par_infeasible = [i for i, st in enumerate(res_par.slot_statuses) if st == Status.INFEASIBLE]
        assert seq_infeasible == par_infeasible and seq_infeasible
        for statuses in (res_seq.slot_statuses, res_par.slot_statuses):
            for i, st in enumerate(statuses):
                if i not in seq_infeasible:
                    assert st in (Status.OPTIMAL, Status.FEASIBLE)
    else:
        assert res_seq.status == Status.OPTIMAL
        assert res_par.status == Status.OPTIMAL
        assert (res_seq.room_count, res_seq.extra_locations, res_seq.unused_capacity) == expected
        assert (res_par.room_count, res_par.extra_locations, res_par.unused_capacity) == expected
        assert res_seq.slot_statuses == [Status.OPTIMAL, Status.OPTIMAL]
        # Verify hard single-location rule per section and exact determinism across 1 vs 2 workers.
        for sec_id in range(problem.num_sections):
            chosen = res_seq.rooms_for_section(sec_id)
            assert chosen == res_par.rooms_for_section(sec_id)
            assert len({locations[r] for r in chosen}) == 1
        assert_allocation(res_seq, schedule, exams, capacities)
