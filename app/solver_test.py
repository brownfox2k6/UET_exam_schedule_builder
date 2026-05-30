"""
Pretests for solver - Heavy Load Performance Version (Strict Indexing)
Updated for Room + ProblemData-based AntColony constructor.
No proctor model.
"""

import os
import sys

current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

import pytest
import solver


@pytest.fixture
def sample_data():
    hp = solver.Hyperparams()
    hp.aco.num_ants = 200
    hp.aco.num_iters = 15

    num_slots = 20
    num_rooms = 8
    num_exams = 120

    timestamps = [1716613200 + (i * 28800) for i in range(num_slots)]

    rooms = [
        solver.Room(
            f"R{i:03d}",
            40 + i * 10,
            "Ton That Thuyet" if i < 4 else "Xuan Thuy",
            "lecture_hall",
        )
        for i in range(num_rooms)
    ]

    room_indices = list(range(num_rooms))

    exams = []

    for i in range(num_exams):
        code = f"UET{i:03d}"
        credits = 1 + (i % 4)

        group_id = i // 10
        students = [
            f"{24000000 + group_id * 100 + idx:08d}"
            for idx in range(15)
        ]

        if i % 3 == 0:
            feasible_slots = list(range(0, num_slots, 2))
        elif i % 3 == 1:
            feasible_slots = list(range(1, num_slots, 2))
        else:
            feasible_slots = list(range(num_slots))

        exams.append(
            solver.Exam(
                code,
                credits,
                students,
                feasible_slots,
                room_indices
            )
        )

    return exams, timestamps, rooms, hp


def test_exam_initialization_and_properties():
    exam = solver.Exam(
        "CS101",
        3,
        ["24000001", "24000002"],
        [0, 1],
        [0]
    )

    assert exam.code == "CS101"
    assert exam.credits == 3
    assert "24000001" in exam.students
    assert exam.feasible_slots == [0, 1]
    assert exam.feasible_rooms == [0]

    exam.credits = 4
    assert exam.credits == 4


def test_room_initialization_and_properties():
    room = solver.Room(
        "101-T",
        80,
        "Ton That Thuyet",
        "lecture_hall",
    )

    assert room.code == "101-T"
    assert room.location == "Ton That Thuyet"
    assert room.type == "lecture_hall"
    assert room.capacity == 80

    room.capacity = 100
    assert room.capacity == 100


def test_hyperparams_nested_values():
    hp = solver.Hyperparams()

    hp.eval.penalty_decay_base = 2.5
    hp.aco.alpha = 1.2
    hp.aco.beta = 1.8
    hp.ls.patience = 50

    assert hp.eval.penalty_decay_base == 2.5
    assert hp.aco.alpha == 1.2
    assert hp.aco.beta == 1.8
    assert hp.ls.patience == 50


def test_hyperparams_default_argument_values():
    eval_p = solver.EvalParams()
    aco_p = solver.AcoParams()
    ls_p = solver.LsParams()

    assert eval_p.penalty_decay_base == 2.0
    assert aco_p.num_ants == 10
    assert ls_p.prob_1_move == 0.5


def test_ant_colony_initialization(sample_data):
    exams, timestamps, rooms, hp = sample_data

    colony = solver.AntColony(
        hp,
        exams,
        timestamps,
        rooms,
        base_seed=42
    )

    assert len(exams) == 120
    assert len(rooms) == 8
    assert colony.global_best_fitness > 0.0


def test_ant_colony_run_one_iteration(sample_data):
    exams, timestamps, rooms, hp = sample_data

    colony = solver.AntColony(
        hp,
        exams,
        timestamps,
        rooms,
        base_seed=100
    )

    cost = colony.run_one_iteration()

    assert isinstance(cost, float)
    assert cost >= 0.0


def test_ant_colony_full_run_with_callback(sample_data):
    exams, timestamps, rooms, hp = sample_data
    hp.aco.num_iters = 5

    colony = solver.AntColony(
        hp,
        exams,
        timestamps,
        rooms,
        base_seed=999
    )

    history = []

    def python_callback(iteration, best_cost):
        history.append((iteration, best_cost))

    colony.run(python_callback)

    assert len(history) == hp.aco.num_iters
    assert history[0][0] == 1
    assert history[-1][0] == hp.aco.num_iters


def test_solution_schedule_integrity(sample_data):
    exams, timestamps, rooms, hp = sample_data

    colony = solver.AntColony(
        hp,
        exams,
        timestamps,
        rooms,
        base_seed=777
    )

    colony.run()
    best_schedule = colony.global_best_schedule

    assert len(best_schedule) == len(exams)

    for slot_assigned in best_schedule:
        assert 0 <= slot_assigned < len(timestamps)


def test_solver_determinism_with_same_seed(sample_data):
    exams, timestamps, rooms, hp = sample_data
    hp.aco.num_ants = 50

    colony_a = solver.AntColony(
        hp,
        exams,
        timestamps,
        rooms,
        base_seed=12345
    )

    colony_b = solver.AntColony(
        hp,
        exams,
        timestamps,
        rooms,
        base_seed=12345
    )

    cost_a = colony_a.run_one_iteration()
    cost_b = colony_b.run_one_iteration()

    assert cost_a == cost_b
    assert colony_a.global_best_schedule == colony_b.global_best_schedule


def test_solver_stochasticity_with_different_seeds(sample_data):
    exams, timestamps, rooms, hp = sample_data
    hp.aco.num_ants = 50

    colony_a = solver.AntColony(
        hp,
        exams,
        timestamps,
        rooms,
        base_seed=111
    )

    colony_b = solver.AntColony(
        hp,
        exams,
        timestamps,
        rooms,
        base_seed=222
    )

    colony_a.run_one_iteration()
    colony_b.run_one_iteration()

    assert (
        colony_a.global_best_fitness != colony_b.global_best_fitness
        or colony_a.global_best_schedule != colony_b.global_best_schedule
    )


def test_edge_case_minimum_credits(sample_data):
    _, timestamps, _, hp = sample_data
    hp.aco.num_ants = 20

    rooms = [
        solver.Room(
            "EDGE-R001",
            30,
            "Ton That Thuyet",
            "lecture_hall",
        )
    ]

    border_exams = [
        solver.Exam(
            "MIL001",
            1,
            ["24000001", "24000002"],
            [0],
            [0]
        ),
        solver.Exam(
            "PE1002",
            1,
            ["24000002", "24000003"],
            [0],
            [0]
        )
    ]

    colony = solver.AntColony(
        hp,
        border_exams,
        timestamps,
        rooms,
        base_seed=42
    )

    assert colony.run_one_iteration() >= 0.0