"""Shared pytest fixtures for solver tests."""

import pytest

import solvers


@pytest.fixture
def large_problem_data():
    hp = solvers.Hyperparams()
    hp.aco.num_ants = 200
    hp.aco.num_iters = 15

    num_slots = 20
    num_rooms = 8
    num_exams = 120

    timestamps = [1716613200 + (i * 28800) for i in range(num_slots)]
    rooms = [
        solvers.Room(
            f"R{i:03d}",
            40 + i * 10,
            "Ton That Thuyet" if i < 4 else "Xuan Thuy",
            "lecture_hall",
        )
        for i in range(num_rooms)
    ]
    room_codes = [room.code for room in rooms]

    exams = []
    for i in range(num_exams):
        code = f"UET{i:03d}"
        credits = 1 + (i % 4)
        group_id = i // 10
        students = [f"{24000000 + group_id * 100 + idx:08d}" for idx in range(15)]
        section = solvers.ExamSection(f"{code}_SEC1", students, room_codes)

        if i % 3 == 0:
            feasible_timestamps = timestamps[0::2]
        elif i % 3 == 1:
            feasible_timestamps = timestamps[1::2]
        else:
            feasible_timestamps = list(timestamps)

        exams.append(solvers.Exam(code, credits, [section], feasible_timestamps))

    return exams, timestamps, rooms, hp
