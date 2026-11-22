"""Shared factories, native crash guards, and exact-oracle helpers for solver tests."""

from pathlib import Path
import subprocess
import sys
import textwrap

import solvers

SECONDS_PER_DAY = 86400.0
DAY = int(SECONDS_PER_DAY)
TIMESTAMPS = [1716613200, 1716642000]
Status = solvers.RoomAssignmentStatus

REPO_ROOT = Path(__file__).resolve().parents[2]


def room_code(index: int) -> str:
    return f"R{index}"


def make_rooms(capacities):
    return [
        solvers.Room(room_code(i), seats, "campus", "lecture")
        for i, seats in enumerate(capacities)
    ]


def make_exam(index, count, feasible_rooms, feasible_timestamps=TIMESTAMPS):
    section = solvers.ExamSection(
        f"E{index}_S0", [f"E{index}_{i}" for i in range(count)], feasible_rooms
    )
    return solvers.Exam(f"E{index}", 3, [section], feasible_timestamps)


def make_problem(exams, capacities):
    return solvers.ProblemData(exams, make_rooms(capacities))


def assign_rooms_for(exams, capacities, schedule):
    return solvers.assign_rooms(
        make_problem(exams, capacities), schedule, solvers.RoomAssignmentParams()
    )


def exam_students(exam):
    students = set()
    for section in exam.sections:
        students.update(section.students)
    return students


def assert_valid_schedule(problem, exams, schedule):
    assert len(schedule) == len(exams)
    rosters = [exam_students(exam) for exam in exams]
    for exam, slot in zip(exams, schedule):
        assert 0 <= slot < problem.num_slots
        assert problem.slot_timestamps[slot] in exam.feasible_timestamps
    for i in range(len(exams)):
        for j in range(i + 1, len(exams)):
            if rosters[i] & rosters[j]:
                assert schedule[i] != schedule[j]


def compute_exact_penalty(problem, exams, schedule, decay_base):
    rosters = [exam_students(exam) for exam in exams]
    total = 0.0
    for i in range(len(exams)):
        ts_i = problem.slot_timestamps[schedule[i]]
        for j in range(i + 1, len(exams)):
            shared = len(rosters[i] & rosters[j])
            if shared == 0:
                continue
            ts_j = problem.slot_timestamps[schedule[j]]
            days = abs(ts_i - ts_j) / SECONDS_PER_DAY
            total += (
                shared * (exams[i].credits + exams[j].credits) * (decay_base ** (-days))
            )
    return total


def assert_empty_assignment(assignment, num_sections, status):
    assert assignment.status == status
    assert assignment.room_count == 0
    assert assignment.unused_capacity == 0
    assert all(assignment.rooms_for_section(i) == [] for i in range(num_sections))


def assert_allocation(assignment, schedule, exams, capacities):
    occupied = set()
    room_count = 0
    unused = 0
    section_id = 0
    for exam_id, exam in enumerate(exams):
        for section in exam.sections:
            selected = assignment.rooms_for_section(section_id)
            assert selected
            assert len(selected) == len(set(selected))
            assert {room_code(room) for room in selected} <= set(section.feasible_rooms)
            seats = sum(capacities[room] for room in selected)
            assert seats >= len(section.students)
            for room in selected:
                key = (schedule[exam_id], room)
                assert key not in occupied
                occupied.add(key)
            room_count += len(selected)
            unused += seats - len(section.students)
            section_id += 1
    assert assignment.room_count == room_count
    assert assignment.unused_capacity == unused


def assert_native_rejects(source: str):
    """Run invalid-input checks in a subprocess so native aborts cannot kill pytest."""
    result = subprocess.run(
        [sys.executable, "-c", textwrap.dedent(source)],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
