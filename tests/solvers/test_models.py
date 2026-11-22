"""Domain objects, hyperparameters, and ProblemData preprocessing contracts."""

import solvers

from tests.solvers.helpers import DAY


def test_exam_and_section_initialization_and_sorting():
    section1 = solvers.ExamSection(
        "CS101_1", ["24000002", "24000001"], ["R002", "R001"]
    )
    section2 = solvers.ExamSection("CS101_2", ["24000003"], ["R002"])

    assert section1.code == "CS101_1"
    assert section1.students == ["24000001", "24000002"]
    assert section1.feasible_rooms == ["R001", "R002"]
    assert section2.students == ["24000003"]
    assert section2.feasible_rooms == ["R002"]

    exam = solvers.Exam("CS101", 3, [section2, section1], [1716642000, 1716613200])

    assert exam.code == "CS101"
    assert exam.credits == 3
    assert [section.code for section in exam.sections] == ["CS101_1", "CS101_2"]
    assert exam.feasible_timestamps == [1716613200, 1716642000]


def test_room_initialization_and_properties():
    default_room = solvers.Room("201-G2", 40)
    assert default_room.code == "201-G2"
    assert default_room.capacity == 40
    assert default_room.location == "default"
    assert default_room.type == "default"

    room = solvers.Room("101-T", 80, "Ton That Thuyet", "lecture_hall")
    assert room.code == "101-T"
    assert room.location == "Ton That Thuyet"
    assert room.type == "lecture_hall"
    assert room.capacity == 80

    room.code = "102-T"
    room.capacity = 100
    room.location = "Xuan Thuy"
    room.type = "computer_lab"
    assert (room.code, room.capacity, room.location, room.type) == (
        "102-T",
        100,
        "Xuan Thuy",
        "computer_lab",
    )


def test_hyperparams_default_values():
    eval_p = solvers.EvalParams()
    aco_p = solvers.AcoParams()
    ls_p = solvers.LsParams()
    room_p = solvers.RoomAssignmentParams()

    assert eval_p.penalty_decay_base == 2.0
    assert (
        aco_p.num_iters,
        aco_p.num_ants,
        aco_p.alpha,
        aco_p.beta,
        aco_p.rho,
        aco_p.tau_min,
        aco_p.tau_max,
        aco_p.max_retries,
    ) == (100, 10, 1.0, 2.0, 0.1, 0.01, 1.0, 100)
    assert (ls_p.prob_1_move, ls_p.max_improvements, ls_p.patience) == (0.5, 10, 100)
    assert (room_p.max_time_seconds, room_p.num_workers, room_p.random_seed) == (30.0, 1, 42)


def test_hyperparams_nested_and_constructor_updates():
    eval_p = solvers.EvalParams()
    eval_p.penalty_decay_base = 2.5
    aco_p = solvers.AcoParams()
    aco_p.alpha = 1.2
    aco_p.beta = 1.8
    aco_p.set_tau_bounds(2.0, 3.0)
    aco_p.set_tau_bounds(0.001, 0.005)
    ls_p = solvers.LsParams()
    ls_p.patience = 50
    room_p = solvers.RoomAssignmentParams()
    room_p.max_time_seconds = 5.0
    room_p.num_workers = 2
    room_p.random_seed = 0

    hp = solvers.Hyperparams(eval_p, aco_p, ls_p, room_p)

    assert hp.eval.penalty_decay_base == 2.5
    assert hp.aco.alpha == 1.2
    assert hp.aco.beta == 1.8
    assert (hp.aco.tau_min, hp.aco.tau_max) == (0.001, 0.005)
    assert hp.ls.patience == 50
    assert (
        hp.room_assignment.max_time_seconds,
        hp.room_assignment.num_workers,
        hp.room_assignment.random_seed,
    ) == (5.0, 2, 0)


def test_returned_collections_are_defensive_copies():
    section = solvers.ExamSection("S0", ["a", "b"], ["R0", "R1"])
    section.students.clear()
    section.feasible_rooms.clear()
    assert section.students == ["a", "b"]
    assert section.feasible_rooms == ["R0", "R1"]

    exam = solvers.Exam("E0", 3, [section], [0, DAY])
    exam.sections.clear()
    exam.feasible_timestamps.clear()
    assert len(exam.sections) == 1
    assert exam.feasible_timestamps == [0, DAY]

    problem = solvers.ProblemData([exam], [solvers.Room("R0", 10), solvers.Room("R1", 10)])
    problem.exam_codes.clear()
    problem.section_codes.clear()
    problem.room_codes.clear()
    problem.slot_timestamps.clear()
    assert problem.exam_codes == ["E0"]
    assert problem.section_codes == ["S0"]
    assert problem.room_codes == ["R0", "R1"]
    assert problem.slot_timestamps == [0, DAY]


def test_problem_dimensions_and_reverse_mappings(large_problem_data):
    exams, timestamps, rooms, _ = large_problem_data

    problem = solvers.ProblemData(exams, rooms)

    assert problem.num_exams == 120
    assert problem.num_sections == 120
    assert problem.num_rooms == 8
    assert problem.num_slots == 20
    assert problem.num_students == 12 * 15
    assert problem.num_enrollments == 120 * 15
    assert problem.exam_codes == [exam.code for exam in exams]
    assert problem.section_codes == [f"UET{i:03d}_SEC1" for i in range(120)]
    assert problem.room_codes == [room.code for room in rooms]
    assert problem.slot_timestamps == timestamps


def test_sparse_and_negative_timestamps_build_sorted_union_slots():
    e0 = solvers.Exam(
        "E0", 2, [solvers.ExamSection("S0", ["shared"], ["R0"])], [2 * DAY, -DAY]
    )
    e1 = solvers.Exam(
        "E1", 4, [solvers.ExamSection("S1", ["shared"], ["R0"])], [2 * DAY, 0]
    )
    problem = solvers.ProblemData([e0, e1], [solvers.Room("R0", 10)])

    assert problem.slot_timestamps == [-DAY, 0, 2 * DAY]
    assert problem.num_slots == 3
