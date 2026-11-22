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
