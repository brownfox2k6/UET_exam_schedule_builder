"""Multithreading, OpenMP parallelism, and GIL-release tests for Python bindings."""

from concurrent.futures import ThreadPoolExecutor
import os
import threading
import time

import pytest

import solvers
from tests.solvers.helpers import DAY

_CPU_COUNT = os.cpu_count() or 1
_SINGLE_THREAD_ENV = os.environ.get("OMP_NUM_THREADS") == "1"


def _build_problem(num_exams: int = 80, num_slots: int = 16, num_rooms: int = 20):
    timestamps = [DAY * (slot + 1) for slot in range(num_slots)]
    rooms = [
        solvers.Room(f"R{index:02d}", 40 + index * 2, f"Building{index % 4}", "lecture")
        for index in range(num_rooms)
    ]
    room_codes = [room.code for room in rooms]

    # Sliding window of 20 students with stride 3 creates degree-12 conflict neighborhoods
    # (max clique size 7 << num_slots), guaranteeing feasibility with rich soft constraints.
    student_pool = num_exams * 3
    exams = []
    for exam_index in range(num_exams):
        students = [f"S{(exam_index * 3 + offset) % student_pool:04d}" for offset in range(20)]
        section = solvers.ExamSection(f"E{exam_index:03d}_S0", students, room_codes)
        exams.append(solvers.Exam(f"E{exam_index:03d}", 1 + (exam_index % 4), [section], timestamps))

    return solvers.ProblemData(exams, rooms)


@pytest.fixture(scope="module")
def shared_problem_and_schedule():
    problem = _build_problem()
    # Balanced conflict-free schedule (5 exams per slot, clique size <= 7 << 16 slots).
    schedule = [exam_index % problem.num_slots for exam_index in range(problem.num_exams)]
    return problem, schedule


def _measure_cpu_ratio(fn):
    wall_start = time.perf_counter()
    cpu_start = time.process_time()
    result = fn()
    cpu_elapsed = time.process_time() - cpu_start
    wall_elapsed = time.perf_counter() - wall_start
    return result, wall_elapsed, cpu_elapsed


def _measure_background_ticks_during(fn) -> tuple[object, int]:
    """Run a pure-Python counter thread while `fn` executes to prove the GIL is released."""
    started = threading.Event()
    stop = threading.Event()
    ticks = 0

    def background_counter():
        nonlocal ticks
        started.set()
        while not stop.is_set():
            ticks += 1

    worker = threading.Thread(target=background_counter)
    worker.start()
    try:
        assert started.wait(timeout=2.0)
        # Wait until the counter thread is actively executing Python bytecode before timing.
        while ticks == 0:
            pass
        ticks_before = ticks
        result = fn()
        ticks_after = ticks
    finally:
        stop.set()
        worker.join(timeout=2.0)

    return result, ticks_after - ticks_before


@pytest.mark.skipif(
    _CPU_COUNT < 2 or _SINGLE_THREAD_ENV,
    reason="Requires at least 2 CPUs and OpenMP not forced to 1 thread",
)
def test_aco_executes_across_multiple_cpu_threads(shared_problem_and_schedule):
    problem, _ = shared_problem_and_schedule
    aco_params = solvers.AcoParams()
    aco_params.num_iters = 120
    aco_params.num_ants = 48
    eval_params = solvers.EvalParams()

    result, wall, cpu = _measure_cpu_ratio(
        lambda: solvers.solve_slots_aco(problem, aco_params, eval_params, base_seed=42)
    )

    assert result.found
    assert wall > 0.0
    assert cpu / wall > 1.2, (
        f"Expected multi-core OpenMP execution for ACO (cpu={cpu:.3f}s, wall={wall:.3f}s, "
        f"ratio={cpu / wall:.2f}x)"
    )


@pytest.mark.skipif(
    _CPU_COUNT < 2 or _SINGLE_THREAD_ENV,
    reason="Requires at least 2 CPUs and OpenMP not forced to 1 thread",
)
def test_assign_rooms_executes_across_multiple_cpu_threads():
    # Balanced schedule across 48 independent active slots (8 exams per slot) gives every slot
    # identical CP-SAT work, proving OPTIMAL quickly while keeping CPU time well above OS timer
    # granularity (including 15.6ms GetProcessTimes ticks on Windows).
    num_exams, num_slots, num_rooms = 384, 48, 24
    problem = _build_problem(num_exams=num_exams, num_slots=num_slots, num_rooms=num_rooms)
    schedule = [exam_index % num_slots for exam_index in range(num_exams)]

    seq_params = solvers.RoomAssignmentParams()
    seq_params.num_workers = 1
    seq_result = solvers.assign_rooms(problem, schedule, seq_params)
    assert seq_result.found

    par_params = solvers.RoomAssignmentParams()
    par_params.num_workers = min(4, _CPU_COUNT)
    par_result, wall, cpu = _measure_cpu_ratio(
        lambda: solvers.assign_rooms(problem, schedule, par_params)
    )

    assert par_result.found
    assert par_result.status == seq_result.status
    assert par_result.room_count == seq_result.room_count
    assert par_result.extra_locations == seq_result.extra_locations
    assert par_result.unused_capacity == seq_result.unused_capacity
    assert wall > 0.0
    assert cpu / wall > 1.2, (
        f"Expected multi-core OpenMP execution for assign_rooms "
        f"(cpu={cpu:.3f}s, wall={wall:.3f}s, ratio={cpu / wall:.2f}x)"
    )


@pytest.mark.parametrize(
    "binding_name",
    ["solve_slots_aco", "improve_slots", "assign_rooms", "complete_timetable", "solve_timetable"],
)
def test_solver_bindings_release_gil(shared_problem_and_schedule, binding_name):
    problem, schedule = shared_problem_and_schedule
    eval_params = solvers.EvalParams()

    if binding_name == "solve_slots_aco":
        aco_params = solvers.AcoParams()
        aco_params.num_iters = 80
        aco_params.num_ants = 32
        result, ticks = _measure_background_ticks_during(
            lambda: solvers.solve_slots_aco(problem, aco_params, eval_params, base_seed=42)
        )
        assert result.found
    elif binding_name == "improve_slots":
        ls_params = solvers.LsParams()
        ls_params.max_improvements = 50_000
        ls_params.patience = 100_000
        result, ticks = _measure_background_ticks_during(
            lambda: solvers.improve_slots(problem, schedule, ls_params, eval_params, base_seed=42)
        )
        assert result.found
    elif binding_name == "assign_rooms":
        room_params = solvers.RoomAssignmentParams()
        room_params.num_workers = 1
        result, ticks = _measure_background_ticks_during(
            lambda: solvers.assign_rooms(problem, schedule, room_params)
        )
        assert result.found
    elif binding_name == "complete_timetable":
        ls_params = solvers.LsParams()
        ls_params.max_improvements = 0
        slot_result = solvers.improve_slots(problem, schedule, ls_params, eval_params, base_seed=42)
        room_params = solvers.RoomAssignmentParams()
        room_params.num_workers = 1
        result, ticks = _measure_background_ticks_during(
            lambda: solvers.complete_timetable(problem, slot_result, room_params, base_seed=42)
        )
        assert result.rooms is not None and result.rooms.found
    else:
        hp = solvers.Hyperparams()
        hp.aco.num_iters = 40
        hp.aco.num_ants = 24
        hp.ls.max_improvements = 200
        hp.ls.patience = 500
        hp.room_assignment.slot_time_limit = 0.5
        hp.room_assignment.num_workers = min(2, _CPU_COUNT)
        result, ticks = _measure_background_ticks_during(
            lambda: solvers.solve_timetable(problem, hp, base_seed=42)
        )
        assert result.slots.found
        assert result.rooms is not None
        assert result.rooms.found

    assert ticks >= 100, (
        f"Background Python thread made insufficient progress ({ticks} ticks) during "
        f"{binding_name}, suggesting the GIL was not released"
    )


def test_concurrent_python_threads_share_problem_data_deterministically(
    shared_problem_and_schedule,
):
    problem, base_schedule = shared_problem_and_schedule
    eval_params = solvers.EvalParams()
    seeds = [11, 22, 33, 44]

    aco_params = solvers.AcoParams()
    aco_params.num_iters = 4
    aco_params.num_ants = 8

    ls_params = solvers.LsParams()
    ls_params.max_improvements = 50
    ls_params.patience = 200

    room_params = solvers.RoomAssignmentParams()
    room_params.num_workers = 2

    def run_pipeline(seed: int):
        events: list[tuple[int, float]] = []
        aco_res = solvers.solve_slots_aco(
            problem,
            aco_params,
            eval_params,
            base_seed=seed,
            callback=lambda iteration, best: events.append((iteration, best)),
        )
        ls_res = solvers.improve_slots(
            problem, base_schedule, ls_params, eval_params, base_seed=seed
        )
        room_res = solvers.assign_rooms(problem, base_schedule, room_params)
        return (
            aco_res.schedule,
            aco_res.penalty,
            events,
            ls_res.schedule,
            ls_res.penalty,
            room_res.status,
            room_res.room_count,
            room_res.extra_locations,
            room_res.unused_capacity,
            [room_res.rooms_for_section(i) for i in range(problem.num_sections)],
        )

    expected = [run_pipeline(seed) for seed in seeds]

    with ThreadPoolExecutor(max_workers=len(seeds)) as executor:
        concurrent = list(executor.map(run_pipeline, seeds))

    assert concurrent == expected
