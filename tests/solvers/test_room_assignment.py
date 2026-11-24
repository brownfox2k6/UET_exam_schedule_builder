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
    assignment = solvers.assign_rooms(make_problem(exams, capacities), [0], params, base_seed=0)

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
    assert_empty_assignment(assignment, Status.INFEASIBLE)


def test_timeout_result_is_independent_of_other_room_assignments():
    exams = [make_exam(0, 5, ["R0", "R1"])]
    problem = make_problem(exams, [8, 5])
    params = solvers.RoomAssignmentParams()
    first = solvers.assign_rooms(problem, [0], params)
    assert first.status == Status.OPTIMAL

    params.slot_time_limit = (
        1e-300  # Deterministically expires before the solver completes.
    )
    assert_empty_assignment(solvers.assign_rooms(problem, [0], params), Status.UNKNOWN)
    assert first.rooms_for_section(0) == [1]

    retried = solvers.assign_rooms(problem, [0], solvers.RoomAssignmentParams())
    assert retried.status == Status.OPTIMAL
    assert retried.rooms_for_section(0) == [1]


def test_reallocation_with_reduced_capacity_reports_infeasible():
    exams = [make_exam(0, 10, ["R0"])]
    assignment = assign_rooms_for(exams, [10], [0])
    assert assignment.status == Status.OPTIMAL
    assert_empty_assignment(assign_rooms_for(exams, [9], [0]), Status.INFEASIBLE)
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


def test_uet_scale_73_rooms_slot_solves_optimally_within_per_slot_budget():
    capacities = [30, 40, 50, 60, 80, 100]
    rooms = [
        solvers.Room(
            f"R{i:02d}",
            capacities[i % len(capacities)],
            "Ton That Thuyet" if (i // len(capacities)) % 2 == 0 else "Kieu Mai",
        )
        for i in range(73)
    ]
    all_codes = [room.code for room in rooms]
    exams = [
        solvers.Exam(
            "E0",
            3,
            [
                solvers.ExamSection("E0_S0", [f"A{i}" for i in range(30)], all_codes),
                solvers.ExamSection("E0_S1", [f"B{i}" for i in range(50)], all_codes),
            ],
            [TIMESTAMPS[0]],
        ),
        solvers.Exam(
            "E1",
            3,
            [
                solvers.ExamSection("E1_S0", [f"C{i}" for i in range(40)], all_codes),
                solvers.ExamSection("E1_S1", [f"D{i}" for i in range(80)], all_codes),
            ],
            [TIMESTAMPS[0]],
        ),
        solvers.Exam(
            "E2",
            3,
            [solvers.ExamSection("E2_S0", [f"F{i}" for i in range(60)], all_codes)],
            [TIMESTAMPS[0]],
        ),
    ]
    problem = solvers.ProblemData(exams, rooms)
    params = solvers.RoomAssignmentParams()
    params.slot_time_limit = 1.0

    assignment = solvers.assign_rooms(problem, [0, 0, 0], params, base_seed=42)
    assert assignment.status == Status.OPTIMAL
    assert (assignment.room_count, assignment.extra_locations, assignment.unused_capacity) == (
        5,
        0,
        0,
    )


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
    assert_empty_assignment(second, Status.INFEASIBLE)
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
        assert_empty_assignment(assignment, Status.INFEASIBLE)
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
    assert_empty_assignment(assignment, Status.INFEASIBLE)


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


def test_single_exam_spanning_three_campuses_reports_two_extra_locations():
    rooms = [
        solvers.Room("R_TTT", 30, "Ton That Thuyet"),
        solvers.Room("R_KM", 40, "Kieu Mai"),
        solvers.Room("R_XT", 50, "Xuan Thuy"),
    ]
    sections = [
        solvers.ExamSection("S0", [f"A{i}" for i in range(25)], ["R_TTT"]),
        solvers.ExamSection("S1", [f"B{i}" for i in range(30)], ["R_KM"]),
        solvers.ExamSection("S2", [f"C{i}" for i in range(45)], ["R_XT"]),
    ]
    exams = [solvers.Exam("E0", 3, sections, [TIMESTAMPS[0]])]
    problem = solvers.ProblemData(exams, rooms)

    assignment = solvers.assign_rooms(problem, [0], solvers.RoomAssignmentParams())
    assert assignment.status == Status.OPTIMAL
    assert (assignment.room_count, assignment.extra_locations, assignment.unused_capacity) == (
        3,
        2,
        20,
    )
    assert_allocation(
        assignment,
        [0],
        exams,
        [room.capacity for room in rooms],
        [room.location for room in rooms],
        problem.room_codes,
    )


def test_saving_one_room_beats_two_extra_locations_and_one_extra_beats_two():
    # Case 1: 3 sections of 60 students. Using all 3 campuses takes 3 rooms (extra_locations=2);
    # consolidating to 2 campuses requires splitting one section across 30+30 -> 4 rooms.
    # Room count priority must pick (3, 2, 0).
    rooms_a = [
        solvers.Room("T1", 60, "TTT"),
        solvers.Room("T2", 30, "TTT"),
        solvers.Room("T3", 30, "TTT"),
        solvers.Room("K1", 60, "KM"),
        solvers.Room("X1", 60, "XT"),
    ]
    codes_a = [room.code for room in rooms_a]
    exams_a = [
        solvers.Exam(
            "E0",
            3,
            [
                solvers.ExamSection(f"S{s}", [f"U{s}_{i}" for i in range(60)], codes_a)
                for s in range(3)
            ],
            [TIMESTAMPS[0]],
        )
    ]
    res_a = solvers.assign_rooms(
        solvers.ProblemData(exams_a, rooms_a), [0], solvers.RoomAssignmentParams()
    )
    assert res_a.status == Status.OPTIMAL
    assert (res_a.room_count, res_a.extra_locations, res_a.unused_capacity) == (3, 2, 0)
    assert_allocation(
        res_a, [0], exams_a, [r.capacity for r in rooms_a], [r.location for r in rooms_a], codes_a
    )

    # Case 2: 3 sections of 30 students, room_count=3 tied.
    # TTT(60)+TTT(60)+KM(30) gives extra_locations=1, unused=60;
    # TTT(60)+KM(30)+XT(30) gives extra_locations=2, unused=30. Consolidation (1 < 2) must win.
    rooms_b = [
        solvers.Room("T1", 60, "TTT"),
        solvers.Room("T2", 60, "TTT"),
        solvers.Room("K1", 30, "KM"),
        solvers.Room("X1", 30, "XT"),
    ]
    codes_b = [room.code for room in rooms_b]
    exams_b = [
        solvers.Exam(
            "E0",
            3,
            [
                solvers.ExamSection(f"S{s}", [f"V{s}_{i}" for i in range(30)], codes_b)
                for s in range(3)
            ],
            [TIMESTAMPS[0]],
        )
    ]
    res_b = solvers.assign_rooms(
        solvers.ProblemData(exams_b, rooms_b), [0], solvers.RoomAssignmentParams()
    )
    assert res_b.status == Status.OPTIMAL
    assert (res_b.room_count, res_b.extra_locations, res_b.unused_capacity) == (3, 1, 60)
    assert_allocation(
        res_b, [0], exams_b, [r.capacity for r in rooms_b], [r.location for r in rooms_b], codes_b
    )


def test_two_multi_section_exams_in_same_slot_consolidate_to_separate_campuses():
    # TTT has 3 rooms of 30; KM has 2 rooms of 30. Two 2-section exams (30 students each) cannot
    # both fit on TTT, so placing one whole exam on TTT and the other on KM yields 0 extra splits.
    rooms = [
        solvers.Room("T0", 30, "TTT"),
        solvers.Room("T1", 30, "TTT"),
        solvers.Room("T2", 30, "TTT"),
        solvers.Room("K0", 30, "KM"),
        solvers.Room("K1", 30, "KM"),
    ]
    codes = [room.code for room in rooms]
    exams = [
        solvers.Exam(
            f"E{e}",
            3,
            [
                solvers.ExamSection(f"E{e}_S{s}", [f"U{e}_{s}_{i}" for i in range(30)], codes)
                for s in range(2)
            ],
            [TIMESTAMPS[0]],
        )
        for e in range(2)
    ]
    problem = solvers.ProblemData(exams, rooms)

    assignment = solvers.assign_rooms(problem, [0, 0], solvers.RoomAssignmentParams())
    assert assignment.status == Status.OPTIMAL
    assert (assignment.room_count, assignment.extra_locations, assignment.unused_capacity) == (
        4,
        0,
        0,
    )
    assert_allocation(
        assignment, [0, 0], exams, [r.capacity for r in rooms], [r.location for r in rooms], codes
    )


@pytest.mark.parametrize(
    "students,expected_rooms,expected_unused",
    [(1, 1, 39), (40, 1, 0), (41, 2, 39), (80, 2, 0), (81, 3, 39)],
)
def test_room_count_boundaries_around_room_capacity_multiples(
    students, expected_rooms, expected_unused
):
    exams = [make_exam(0, students, ["R0", "R1", "R2"])]
    capacities = [40, 40, 40]
    assignment = assign_rooms_for(exams, capacities, [0])

    assert assignment.status == Status.OPTIMAL
    assert (assignment.room_count, assignment.extra_locations, assignment.unused_capacity) == (
        expected_rooms,
        0,
        expected_unused,
    )
    assert_allocation(assignment, [0], exams, capacities)


def test_room_assignment_surface_omits_slot_statuses_and_model_invalid():
    assert not hasattr(Status, "MODEL_INVALID")
    assert {Status.OPTIMAL, Status.FEASIBLE, Status.UNKNOWN, Status.INFEASIBLE} == set(Status)

    assignment = assign_rooms_for([make_exam(0, 5, ["R0"])], [10], [0])
    assert assignment.status == Status.OPTIMAL
    assert not hasattr(assignment, "slot_statuses")


def test_any_infeasible_slot_fails_overall_assignment_even_with_feasible_and_empty_slots():
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
    assert_empty_assignment(assignment, Status.INFEASIBLE)


@pytest.mark.parametrize("num_workers", [1, 2, 4])
@pytest.mark.parametrize("schedule", [[0, 1], [1, 0]])
def test_infeasible_and_timed_out_slots_always_report_infeasible(num_workers, schedule):
    # One slot times out (UNKNOWN) under a tiny limit; the other is provably INFEASIBLE (20 > 10).
    # Overall status must deterministically be INFEASIBLE regardless of slot order or workers.
    ts = [1000, 2000]
    exams = [
        make_exam(0, 5, ["R0"], ts),
        make_exam(1, 20, ["R0"], ts),
    ]
    problem = make_problem(exams, [10])
    params = solvers.RoomAssignmentParams()
    params.slot_time_limit = 1e-300
    params.num_workers = num_workers

    assignment = solvers.assign_rooms(problem, schedule, params)
    assert_empty_assignment(assignment, Status.INFEASIBLE)


@pytest.mark.parametrize("num_workers", [1, 4])
def test_interleaved_multi_section_slots_and_empty_slots_scatter_rooms_correctly(num_workers):
    # 5 slots (slots 1 and 3 are empty), exams assigned out-of-order across slots:
    # E0 (3 sections) -> slot 4, E1 (2 sections) -> slot 0, E2 (2 sections) -> slot 2.
    ts = [0, 1000, 2000, 3000, 4000]
    rooms = [
        solvers.Room("R0", 40, "TTT"),
        solvers.Room("R1", 40, "KM"),
        solvers.Room("R2", 40, "XT"),
    ]
    exams = [
        solvers.Exam(
            "E0",
            3,
            [
                solvers.ExamSection("E0_S0", ["a0"], ["R0"]),
                solvers.ExamSection("E0_S1", ["a1"], ["R1"]),
                solvers.ExamSection("E0_S2", ["a2"], ["R2"]),
            ],
            ts,
        ),
        solvers.Exam(
            "E1",
            3,
            [
                solvers.ExamSection("E1_S0", ["b0"], ["R2"]),
                solvers.ExamSection("E1_S1", ["b1"], ["R0"]),
            ],
            ts,
        ),
        solvers.Exam(
            "E2",
            3,
            [
                solvers.ExamSection("E2_S0", ["c0"], ["R1"]),
                solvers.ExamSection("E2_S1", ["c1"], ["R2"]),
            ],
            ts,
        ),
    ]
    problem = solvers.ProblemData(exams, rooms)
    params = solvers.RoomAssignmentParams()
    params.num_workers = num_workers
    schedule = [4, 0, 2]

    assignment = solvers.assign_rooms(problem, schedule, params, base_seed=7)
    assert assignment.status == Status.OPTIMAL
    assert problem.section_to_exam == [0, 0, 0, 1, 1, 2, 2]
    assert [assignment.rooms_for_section(i) for i in range(problem.num_sections)] == [
        [0],
        [1],
        [2],
        [2],
        [0],
        [1],
        [2],
    ]
    assert_allocation(
        assignment,
        schedule,
        exams,
        [r.capacity for r in rooms],
        [r.location for r in rooms],
        problem.room_codes,
    )


@pytest.mark.parametrize("base_seed", [0, 2**31 - 1, 2**31, 2**64 - 1])
def test_extreme_64bit_base_seeds_and_workers_exceeding_slot_count(base_seed):
    ts = [0, 1000, 2000]
    exams = [
        make_exam(0, 15, ["R0", "R1"], ts),
        make_exam(1, 25, ["R0", "R1"], ts),
        make_exam(2, 10, ["R0", "R1"], ts),
    ]
    problem = make_problem(exams, [30, 20])
    schedule = [0, 1, 2]

    params_seq = solvers.RoomAssignmentParams()
    params_seq.num_workers = 1
    params_many = solvers.RoomAssignmentParams()
    params_many.num_workers = 8  # Exceeds problem.num_slots (3).

    res_seq = solvers.assign_rooms(problem, schedule, params_seq, base_seed=base_seed)
    res_many = solvers.assign_rooms(problem, schedule, params_many, base_seed=base_seed)

    assert res_seq.status == Status.OPTIMAL
    assert res_many.status == Status.OPTIMAL
    assert [res_seq.rooms_for_section(i) for i in range(3)] == [
        res_many.rooms_for_section(i) for i in range(3)
    ]
    assert_allocation(res_many, schedule, exams, [30, 20])


def test_feasible_timeout_and_optimal_unknown_mix_preserve_exact_contracts():
    # Slot 0: 18 multi-section exams across 45 prime-capacity rooms & 3 campuses (finds a valid
    # solution quickly, but symmetry prevents proving OPTIMAL within 0.35s).
    # Slot 1: trivial 1-section exam that proves OPTIMAL in <1ms.
    caps = [31, 33, 37, 41, 43, 47, 53, 59, 61]
    rooms = [solvers.Room(f"R{i:02d}", caps[i % len(caps)], f"L{i % 3}") for i in range(45)]
    codes = [room.code for room in rooms]
    exams = [
        solvers.Exam(
            f"E{e}",
            3,
            [
                solvers.ExamSection(
                    f"E{e}_S{s}",
                    [f"u_{e}_{s}_{k}" for k in range(25 + (e * 5 + s * 7) % 25)],
                    codes,
                )
                for s in range(2)
            ],
            TIMESTAMPS,
        )
        for e in range(18)
    ]
    exams.append(solvers.Exam("EASY", 3, [solvers.ExamSection("EASY_S0", ["x"], ["R00"])], TIMESTAMPS))
    problem = solvers.ProblemData(exams, rooms)
    schedule = [0] * 18 + [1]

    # Case A: OPTIMAL (slot 1) + UNKNOWN (slot 0 under tiny budget) -> empty UNKNOWN overall.
    tiny_params = solvers.RoomAssignmentParams()
    tiny_params.slot_time_limit = 0.005
    tiny_params.num_workers = 2
    timed_out = solvers.assign_rooms(problem, schedule, tiny_params, base_seed=42)
    assert_empty_assignment(timed_out, Status.UNKNOWN)

    # Case B: FEASIBLE (slot 0) + OPTIMAL (slot 1) -> FEASIBLE overall with exact metrics and
    # every hard constraint satisfied across all sections.
    feasible_params = solvers.RoomAssignmentParams()
    feasible_params.slot_time_limit = 0.35
    feasible_params.num_workers = 2
    feasible = solvers.assign_rooms(problem, schedule, feasible_params, base_seed=42)
    assert feasible.status == Status.FEASIBLE
    assert feasible.found
    assert feasible.rooms_for_section(36) == [0]
    assert_allocation(
        feasible,
        schedule,
        exams,
        [r.capacity for r in rooms],
        [r.location for r in rooms],
        codes,
    )


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
        assert_empty_assignment(res_seq, Status.INFEASIBLE)
        assert_empty_assignment(res_par, Status.INFEASIBLE)
    else:
        assert res_seq.status == Status.OPTIMAL
        assert res_par.status == Status.OPTIMAL
        assert (res_seq.room_count, res_seq.extra_locations, res_seq.unused_capacity) == expected
        assert (res_par.room_count, res_par.extra_locations, res_par.unused_capacity) == expected
        # Verify hard single-location rule per section and exact determinism across 1 vs 2 workers.
        for sec_id in range(problem.num_sections):
            chosen = res_seq.rooms_for_section(sec_id)
            assert chosen == res_par.rooms_for_section(sec_id)
            assert len({locations[r] for r in chosen}) == 1
        assert_allocation(res_seq, schedule, exams, capacities, locations)
