"""Input, parameter, and schedule validation contracts across the Python API."""

import pytest

import solvers

from tests.solvers.helpers import TIMESTAMPS, assert_native_rejects, assign_rooms_for, make_exam


# --- Hyperparameter validation ---


@pytest.mark.parametrize("field", ["num_iters", "num_ants", "max_retries"])
@pytest.mark.parametrize("value", [0, -1])
def test_aco_rejects_nonpositive_counts_without_mutating(field, value):
    params = solvers.AcoParams()
    previous = getattr(params, field)
    with pytest.raises(ValueError, match=field):
        setattr(params, field, value)
    assert getattr(params, field) == previous


@pytest.mark.parametrize(
    "field,value",
    [(field, value) for field in ("alpha", "beta") for value in (-1.0, float("nan"), float("inf"))]
    + [("rho", value) for value in (-0.1, 1.1, float("nan"), float("inf"))]
    + [("tau_min", value) for value in (0.0, -1.0, 1.0, float("nan"), float("inf"))]
    + [("tau_max", value) for value in (0.0, 0.01, float("nan"), float("inf"))],
)
def test_aco_rejects_invalid_real_parameters_without_mutating(field, value):
    params = solvers.AcoParams()
    previous = getattr(params, field)
    with pytest.raises(ValueError, match=field):
        setattr(params, field, value)
    assert getattr(params, field) == previous


@pytest.mark.parametrize(
    "bounds",
    [(0.0, 1.0), (1.0, 1.0), (2.0, 1.0), (0.1, float("inf")), (float("nan"), 1.0)],
)
def test_rejected_pheromone_bounds_preserve_both_previous_values(bounds):
    params = solvers.AcoParams()
    params.set_tau_bounds(2.0, 3.0)
    with pytest.raises(ValueError, match="tau_"):
        params.set_tau_bounds(*bounds)
    assert (params.tau_min, params.tau_max) == (2.0, 3.0)


@pytest.mark.parametrize("value", [-1.0, 0.0, 1.0, float("nan"), float("inf")])
def test_invalid_decay_base_preserves_previous_value(value):
    params = solvers.EvalParams()
    with pytest.raises(ValueError, match="penalty_decay_base"):
        params.penalty_decay_base = value
    assert params.penalty_decay_base == 2.0


@pytest.mark.parametrize(
    "field,value",
    [("prob_1_move", value) for value in (-0.1, 1.1, float("nan"), float("inf"))]
    + [("max_improvements", -1), ("patience", -1)],
)
def test_local_search_rejects_invalid_parameters_without_mutating(field, value):
    params = solvers.LsParams()
    previous = getattr(params, field)
    with pytest.raises(ValueError, match=field):
        setattr(params, field, value)
    assert getattr(params, field) == previous


@pytest.mark.parametrize(
    "field,value",
    [("slot_time_limit", value) for value in (0, -1, float("nan"), float("inf"))]
    + [("num_workers", 0), ("num_workers", -1)],
)
def test_room_assignment_rejects_invalid_parameters_without_mutating(field, value):
    params = solvers.RoomAssignmentParams()
    previous = getattr(params, field)
    with pytest.raises(ValueError, match=field):
        setattr(params, field, value)
    assert getattr(params, field) == previous


# --- Domain model validation ---


@pytest.mark.parametrize(
    "code,capacity,location,room_type,match",
    [
        ("", 10, "campus", "lecture", "code"),
        ("R", 0, "campus", "lecture", "capacity"),
        ("R", -5, "campus", "lecture", "capacity"),
        ("R", 10, "", "lecture", "location"),
        ("R", 10, "campus", "", "type"),
    ],
)
def test_room_rejects_invalid_constructor_arguments(code, capacity, location, room_type, match):
    with pytest.raises(ValueError, match=match):
        solvers.Room(code, capacity, location, room_type)


@pytest.mark.parametrize(
    "field,value",
    [("code", ""), ("capacity", 0), ("capacity", -1), ("location", ""), ("type", "")],
)
def test_invalid_room_updates_leave_previous_values_intact(field, value):
    room = solvers.Room("R", 10, "campus", "lecture")
    previous = getattr(room, field)
    with pytest.raises(ValueError, match=field):
        setattr(room, field, value)
    assert getattr(room, field) == previous


@pytest.mark.parametrize(
    "code,students,feasible_rooms,match",
    [
        ("", ["a"], ["R0"], "Section code"),
        ("S", [], ["R0"], "students"),
        ("S", [""], ["R0"], "students"),
        ("S", ["a", "a"], ["R0"], "students"),
        ("S", ["a"], [], "feasible_rooms"),
        ("S", ["a"], [""], "feasible_rooms"),
        ("S", ["a"], ["R0", "R0"], "feasible_rooms"),
    ],
)
def test_exam_section_rejects_invalid_constructor_arguments(code, students, feasible_rooms, match):
    with pytest.raises(ValueError, match=match):
        solvers.ExamSection(code, students, feasible_rooms)


@pytest.mark.parametrize(
    "field,value",
    [("code", "S2"), ("students", ["b"]), ("feasible_rooms", ["R1"])],
)
def test_exam_section_properties_are_read_only(field, value):
    section = solvers.ExamSection("S1", ["a"], ["R0"])
    with pytest.raises(AttributeError):
        setattr(section, field, value)


@pytest.mark.parametrize(
    "code,credits,feasible_timestamps,match",
    [
        ("", 3, [0], "Exam code"),
        ("E", 0, [0], "credits"),
        ("E", -1, [0], "credits"),
        ("E", 3, [], "feasible_timestamps"),
        ("E", 3, [100, 100], "feasible_timestamps"),
        ("E", 3, [200, 100, 200], "feasible_timestamps"),
    ],
)
def test_exam_rejects_invalid_constructor_arguments(code, credits, feasible_timestamps, match):
    section = solvers.ExamSection("S", ["student"], ["R0"])
    with pytest.raises(ValueError, match=match):
        solvers.Exam(code, credits, [section], feasible_timestamps)


@pytest.mark.parametrize("case", ["empty", "duplicate_codes", "overlapping_students"])
def test_exam_rejects_invalid_sections(case):
    candidates = {
        "empty": [],
        "duplicate_codes": [
            solvers.ExamSection("same", ["a"], ["R0"]),
            solvers.ExamSection("same", ["b", "c"], ["R0"]),
        ],
        "overlapping_students": [
            solvers.ExamSection("A", ["a"], ["R0"]),
            solvers.ExamSection("B", ["a", "b"], ["R0"]),
        ],
    }
    with pytest.raises(ValueError):
        solvers.Exam("E", 3, candidates[case], [0])


@pytest.mark.parametrize(
    "field,value",
    [
        ("code", "E2"),
        ("credits", 4),
        ("sections", [solvers.ExamSection("S2", ["b"], ["R0"])]),
        ("feasible_timestamps", [100]),
    ],
)
def test_exam_properties_are_read_only(field, value):
    exam = solvers.Exam("E1", 3, [solvers.ExamSection("S1", ["a"], ["R0"])], [0])
    with pytest.raises(AttributeError):
        setattr(exam, field, value)


# --- ProblemData validation ---


@pytest.mark.parametrize("argument", ["exams", "rooms"])
def test_problem_rejects_empty_dimensions_without_crashing(argument):
    assert_native_rejects(
        f"""
        import pytest
        import solvers

        exam = solvers.Exam("E", 3, [solvers.ExamSection("S", ["student"], ["R0"])], [0])
        kwargs = dict(exams=[exam], rooms=[solvers.Room("R0", 10)])
        kwargs[{argument!r}] = []
        with pytest.raises(ValueError, match={f'{argument} must not be empty'!r}):
            solvers.ProblemData(**kwargs)
        """
    )


def test_problem_rejects_duplicate_exam_codes():
    exams = [
        solvers.Exam("same", 3, [solvers.ExamSection("S0", ["a"], ["R0"])], [0]),
        solvers.Exam("same", 3, [solvers.ExamSection("S1", ["b"], ["R0"])], [0]),
    ]
    with pytest.raises(ValueError, match="duplicate exam code"):
        solvers.ProblemData(exams, [solvers.Room("R0", 10)])


def test_problem_rejects_duplicate_room_codes_without_crashing():
    assert_native_rejects(
        """
        import pytest
        import solvers

        exam = solvers.Exam("E", 3, [solvers.ExamSection("S", ["student"], ["R0"])], [0])
        rooms = [solvers.Room("R0", 10), solvers.Room("R0", 20)]
        with pytest.raises(ValueError, match="duplicate room code"):
            solvers.ProblemData([exam], rooms)
        """
    )


def test_problem_rejects_unknown_feasible_room_code_without_crashing():
    assert_native_rejects(
        """
        import pytest
        import solvers

        exam = solvers.Exam("E", 3, [solvers.ExamSection("S", ["student"], ["MISSING"])], [0])
        with pytest.raises(ValueError, match="unknown feasible room code"):
            solvers.ProblemData([exam], [solvers.Room("R0", 10)])
        """
    )


# --- Schedule validation ---


@pytest.mark.parametrize("schedule", [[], [0, 0], [-1], [1]])
def test_local_search_rejects_invalid_slots_and_remains_reusable(schedule):
    assert_native_rejects(
        f"""
        import pytest
        import solvers

        section = solvers.ExamSection("S", ["student"], ["R0"])
        exam = solvers.Exam("E", 3, [section], [0])
        params = solvers.Hyperparams()
        params.ls.max_improvements = 0
        problem = solvers.ProblemData([exam], [solvers.Room("R0", 10)])
        with pytest.raises(ValueError):
            solvers.improve_slots(problem, {schedule!r}, params.ls, params.eval)
        assert solvers.improve_slots(problem, [0], params.ls, params.eval).schedule == [0]
        """
    )


def test_local_search_rejects_ineligible_slots_and_student_conflicts():
    assert_native_rejects(
        """
        import pytest
        import solvers

        exams = [
            solvers.Exam("E0", 3, [solvers.ExamSection("S0", ["shared"], ["R0"])], [0]),
            solvers.Exam("E1", 3, [solvers.ExamSection("S1", ["shared"], ["R0"])], [0, 28800]),
        ]
        params = solvers.Hyperparams()
        params.ls.max_improvements = 0
        problem = solvers.ProblemData(exams, [solvers.Room("R0", 10), solvers.Room("R1", 10)])
        for schedule in ([1, 0], [0, 0]):
            with pytest.raises(ValueError):
                solvers.improve_slots(problem, schedule, params.ls, params.eval)
        assert solvers.improve_slots(problem, [0, 1], params.ls, params.eval).schedule == [0, 1]
        """
    )


@pytest.mark.parametrize("schedule", [[], [0, 1], [-1], [2]])
def test_assign_rooms_rejects_incomplete_or_out_of_range_schedule(schedule):
    with pytest.raises(ValueError):
        assign_rooms_for([make_exam(0, 5, ["R0"])], [10], schedule)


def test_assign_rooms_rejects_an_ineligible_slot():
    exams = [
        make_exam(0, 5, ["R0"], [TIMESTAMPS[0]]),
        make_exam(1, 5, ["R0"], [TIMESTAMPS[1]]),
    ]
    with pytest.raises(ValueError, match="infeasible slot"):
        assign_rooms_for(exams, [10], [1, 1])


def test_assign_rooms_rejects_student_conflicts():
    exams = [
        solvers.Exam(f"E{i}", 3, [solvers.ExamSection(f"S{i}", ["shared"], ["R0"])], TIMESTAMPS)
        for i in range(2)
    ]
    with pytest.raises(ValueError, match="conflict"):
        assign_rooms_for(exams, [10, 10], [0, 0])
