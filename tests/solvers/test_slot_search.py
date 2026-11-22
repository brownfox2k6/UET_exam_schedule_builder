"""ACO and Local Search slot-assignment contracts and numerical stability tests."""

from math import isfinite

import pytest

import solvers

from tests.solvers.helpers import DAY, assert_valid_schedule, compute_exact_penalty

# --- ACO ---


def test_aco_run_one_iteration(large_problem_data):
    exams, _, rooms, hp = large_problem_data
    hp.aco.num_iters = 1

    problem = solvers.ProblemData(exams, rooms)
    result = solvers.solve_slots_aco(problem, hp.aco, hp.eval, base_seed=100)

    assert result.found
    assert isinstance(result.penalty, float)
    assert result.penalty >= 0.0


def test_aco_progress_callback_is_monotone(large_problem_data):
    exams, _, rooms, hp = large_problem_data
    hp.aco.num_iters = 5

    problem = solvers.ProblemData(exams, rooms)
    history = []

    def python_callback(iteration, best_cost):
        history.append((iteration, best_cost))

    result = solvers.solve_slots_aco(
        problem, hp.aco, hp.eval, base_seed=999, callback=python_callback
    )

    iterations, penalties = zip(*history)
    assert iterations == tuple(range(1, hp.aco.num_iters + 1))
    assert penalties == tuple(sorted(penalties, reverse=True))
    assert penalties[-1] == result.penalty


def test_aco_schedule_is_valid_and_matches_exact_penalty(large_problem_data):
    exams, _, rooms, hp = large_problem_data

    problem = solvers.ProblemData(exams, rooms)
    result = solvers.solve_slots_aco(problem, hp.aco, hp.eval, base_seed=777)
    assert result.found

    assert_valid_schedule(problem, exams, result.schedule)
    expected_penalty = compute_exact_penalty(
        problem, exams, result.schedule, hp.eval.penalty_decay_base
    )
    assert result.penalty == pytest.approx(expected_penalty)


def test_aco_determinism_with_same_seed(large_problem_data):
    exams, _, rooms, hp = large_problem_data
    hp.aco.num_ants = 50
    hp.aco.num_iters = 1

    problem = solvers.ProblemData(exams, rooms)
    result_a = solvers.solve_slots_aco(problem, hp.aco, hp.eval, base_seed=12345)
    result_b = solvers.solve_slots_aco(problem, hp.aco, hp.eval, base_seed=12345)

    assert result_a.found and result_b.found
    assert result_a.penalty == result_b.penalty
    assert result_a.schedule == result_b.schedule


def test_aco_stochasticity_with_different_seeds(large_problem_data):
    exams, _, rooms, hp = large_problem_data
    hp.aco.num_ants = 50
    hp.aco.num_iters = 1

    problem = solvers.ProblemData(exams, rooms)
    result_a = solvers.solve_slots_aco(problem, hp.aco, hp.eval, base_seed=111)
    result_b = solvers.solve_slots_aco(problem, hp.aco, hp.eval, base_seed=222)

    assert result_a.found and result_b.found
    assert (result_a.penalty, result_a.schedule) != (
        result_b.penalty,
        result_b.schedule,
    )


@pytest.mark.parametrize(
    "tau_bounds,alpha,beta",
    [
        (
            (2.0, 3.0),
            1000.0,
            1.0,
        ),  # Would overflow pow(tau, alpha) without log-space shifting.
        (
            (0.01, 0.02),
            200.0,
            2.0,
        ),  # Would underflow pow(tau, alpha) without log-space shifting.
        ((0.01, 0.02), 5.0, 200.0),  # Small weights with strong heuristic preference.
    ],
)
def test_aco_log_space_weights_handle_extreme_parameters(tau_bounds, alpha, beta):
    exams = [
        solvers.Exam("E0", 3, [solvers.ExamSection("S0", ["shared"], ["R0"])], [0]),
        solvers.Exam(
            "E1", 3, [solvers.ExamSection("S1", ["shared"], ["R0"])], [DAY, 30 * DAY]
        ),
    ]
    problem = solvers.ProblemData(exams, [solvers.Room("R0", 10)])
    aco = solvers.AcoParams()
    aco.num_ants = 10
    aco.num_iters = 3
    aco.set_tau_bounds(*tau_bounds)
    aco.alpha = alpha
    aco.beta = beta
    eval_p = solvers.EvalParams()

    result = solvers.solve_slots_aco(problem, aco, eval_p, base_seed=42)
    assert result.found
    assert isfinite(result.penalty)
    assert_valid_schedule(problem, exams, result.schedule)
    assert result.penalty == pytest.approx(
        compute_exact_penalty(
            problem, exams, result.schedule, eval_p.penalty_decay_base
        )
    )
    if beta >= 200.0:
        assert result.schedule == [0, 2]


def test_aco_callback_exception_propagates_and_solver_remains_reusable():
    params = solvers.Hyperparams()
    params.aco.num_ants = 2
    params.aco.num_iters = 1
    exam = solvers.Exam("E", 3, [solvers.ExamSection("S", ["student"], ["R0"])], [0])
    problem = solvers.ProblemData([exam], [solvers.Room("R0", 10)])

    def failing_callback(iteration, penalty):
        raise RuntimeError("callback failed")

    with pytest.raises(RuntimeError, match="callback failed"):
        solvers.solve_slots_aco(
            problem, params.aco, params.eval, callback=failing_callback
        )
    assert solvers.solve_slots_aco(problem, params.aco, params.eval).schedule == [0]


def test_aco_solves_tight_graph_coloring_with_multi_section_conflicts():
    # 3-clique formed via students spread across different sections; only 3 slots exist.
    e0 = solvers.Exam(
        "E0",
        2,
        [
            solvers.ExamSection("E0_S1", ["s01"], ["R0"]),
            solvers.ExamSection("E0_S2", ["s02"], ["R0"]),
        ],
        [0, DAY, 2 * DAY],
    )
    e1 = solvers.Exam(
        "E1",
        3,
        [
            solvers.ExamSection("E1_S1", ["s01"], ["R0"]),
            solvers.ExamSection("E1_S2", ["s12", "s12_b"], ["R0"]),
        ],
        [0, DAY, 2 * DAY],
    )
    e2 = solvers.Exam(
        "E2",
        4,
        [
            solvers.ExamSection("E2_S1", ["s02", "s12"], ["R0"]),
            solvers.ExamSection("E2_S2", ["s12_b"], ["R0"]),
        ],
        [0, DAY, 2 * DAY],
    )
    exams = [e0, e1, e2]
    problem = solvers.ProblemData(exams, [solvers.Room("R0", 10)])
    aco = solvers.AcoParams()
    aco.num_ants = 10
    aco.num_iters = 5
    eval_p = solvers.EvalParams()

    result = solvers.solve_slots_aco(problem, aco, eval_p, base_seed=7)
    assert result.found
    assert len(set(result.schedule)) == 3
    assert_valid_schedule(problem, exams, result.schedule)
    assert result.penalty == pytest.approx(
        compute_exact_penalty(problem, exams, result.schedule, eval_p.penalty_decay_base)
    )


def test_scoring_handles_large_64bit_timestamp_gaps_without_overflow():
    far_past = -(10**12)
    far_future = 10**12
    exams = [
        solvers.Exam("E0", 3, [solvers.ExamSection("S0", ["shared"], ["R0"])], [far_past]),
        solvers.Exam("E1", 3, [solvers.ExamSection("S1", ["shared"], ["R0"])], [far_future]),
    ]
    problem = solvers.ProblemData(exams, [solvers.Room("R0", 10)])
    ls = solvers.LsParams()
    ls.max_improvements = 0
    result = solvers.improve_slots(problem, [0, 1], ls, solvers.EvalParams())
    assert result.found
    assert isfinite(result.penalty)
    assert result.penalty == pytest.approx(0.0)


# --- Local Search ---


def test_local_search_1_move_improves_schedule_and_matches_exact_penalty():
    exams = [
        solvers.Exam("E0", 3, [solvers.ExamSection("S0", ["shared"], ["R0"])], [0]),
        solvers.Exam(
            "E1", 4, [solvers.ExamSection("S1", ["shared"], ["R0"])], [DAY, 10 * DAY]
        ),
    ]
    problem = solvers.ProblemData(exams, [solvers.Room("R0", 10)])
    ls = solvers.LsParams()
    ls.prob_1_move = 1.0
    ls.max_improvements = 10
    ls.patience = 100
    eval_p = solvers.EvalParams()

    improved = solvers.improve_slots(problem, [0, 1], ls, eval_p, base_seed=42)
    assert improved.found
    assert improved.schedule == [0, 2]
    assert_valid_schedule(problem, exams, improved.schedule)
    assert improved.penalty == pytest.approx(
        compute_exact_penalty(
            problem, exams, improved.schedule, eval_p.penalty_decay_base
        )
    )


def test_local_search_2_swap_improves_conflicting_exams():
    # E0 conflicts with both E1 (fixed at 10*DAY) and E2. Swapping E0 <-> E2 moves high-credit E0
    # away from E1 while keeping |slot(E0) - slot(E2)| unchanged.
    exams = [
        solvers.Exam(
            "E0", 10, [solvers.ExamSection("S0", ["s01", "s02"], ["R0"])], [0, 9 * DAY]
        ),
        solvers.Exam(
            "E1", 10, [solvers.ExamSection("S1", ["s01"], ["R0"])], [10 * DAY]
        ),
        solvers.Exam(
            "E2", 1, [solvers.ExamSection("S2", ["s02"], ["R0"])], [0, 9 * DAY]
        ),
    ]
    problem = solvers.ProblemData(exams, [solvers.Room("R0", 10)])
    ls = solvers.LsParams()
    ls.prob_1_move = 0.0
    ls.max_improvements = 10
    ls.patience = 200
    eval_p = solvers.EvalParams()

    initial_penalty = compute_exact_penalty(
        problem, exams, [1, 2, 0], eval_p.penalty_decay_base
    )
    improved = solvers.improve_slots(problem, [1, 2, 0], ls, eval_p, base_seed=42)

    assert improved.found
    assert improved.schedule == [0, 2, 1]
    assert improved.penalty < initial_penalty
    assert_valid_schedule(problem, exams, improved.schedule)
    assert improved.penalty == pytest.approx(
        compute_exact_penalty(
            problem, exams, improved.schedule, eval_p.penalty_decay_base
        )
    )


def test_local_search_2_swap_improves_non_conflicting_exams():
    exams = [
        solvers.Exam("E0", 1, [solvers.ExamSection("S0", ["s1", "s2"], ["R0"])], [0]),
        solvers.Exam(
            "E1", 10, [solvers.ExamSection("S1", ["s1"], ["R0"])], [DAY, 10 * DAY]
        ),
        solvers.Exam(
            "E2", 1, [solvers.ExamSection("S2", ["s2"], ["R0"])], [DAY, 10 * DAY]
        ),
    ]
    problem = solvers.ProblemData(exams, [solvers.Room("R0", 10)])
    ls = solvers.LsParams()
    ls.prob_1_move = 0.0
    ls.max_improvements = 10
    ls.patience = 200
    eval_p = solvers.EvalParams()

    improved = solvers.improve_slots(problem, [0, 1, 2], ls, eval_p, base_seed=42)
    assert improved.found
    assert improved.schedule == [0, 2, 1]
    assert_valid_schedule(problem, exams, improved.schedule)
    assert improved.penalty == pytest.approx(
        compute_exact_penalty(
            problem, exams, improved.schedule, eval_p.penalty_decay_base
        )
    )


@pytest.mark.parametrize("prob_1_move", [1.0, 0.0])
def test_local_search_rejects_flat_zero_delta_moves(prob_1_move):
    exams = [
        solvers.Exam("E0", 3, [solvers.ExamSection("S0", ["a"], ["R0"])], [0, DAY]),
        solvers.Exam("E1", 3, [solvers.ExamSection("S1", ["b"], ["R0"])], [0, DAY]),
    ]
    problem = solvers.ProblemData(exams, [solvers.Room("R0", 10)])
    ls = solvers.LsParams()
    ls.prob_1_move = prob_1_move
    ls.max_improvements = 20
    ls.patience = 50
    eval_p = solvers.EvalParams()

    improved = solvers.improve_slots(problem, [0, 1], ls, eval_p, base_seed=123)
    assert improved.schedule == [0, 1]
    assert improved.penalty == 0.0


def test_local_search_maintains_exact_penalty_and_determinism_on_large_problem(
    large_problem_data,
):
    exams, _, rooms, hp = large_problem_data
    hp.aco.num_ants = 20
    hp.aco.num_iters = 2
    hp.ls.prob_1_move = 0.5
    hp.ls.max_improvements = 200
    hp.ls.patience = 500

    problem = solvers.ProblemData(exams, rooms)
    initial = solvers.solve_slots_aco(problem, hp.aco, hp.eval, base_seed=77)
    assert initial.found

    improved = solvers.improve_slots(
        problem, initial.schedule, hp.ls, hp.eval, base_seed=88
    )
    assert improved.found
    assert improved.penalty <= initial.penalty + 1e-12
    assert_valid_schedule(problem, exams, improved.schedule)
    assert improved.penalty == pytest.approx(
        compute_exact_penalty(
            problem, exams, improved.schedule, hp.eval.penalty_decay_base
        )
    )

    repeated = solvers.improve_slots(
        problem, initial.schedule, hp.ls, hp.eval, base_seed=88
    )
    assert repeated.schedule == improved.schedule
    assert repeated.penalty == improved.penalty


def test_local_search_2_swap_rejects_swaps_violating_third_party_conflicts_or_feasibility():
    # E0 (slot 0) and E1 (slot 2) would benefit from swapping, but:
    # - Case A: E2 (fixed at slot 2) conflicts with E0, blocking E0 -> slot 2.
    exams_conflict = [
        solvers.Exam("E0", 10, [solvers.ExamSection("S0", ["s02", "s03"], ["R0"])], [0, 9 * DAY]),
        solvers.Exam("E1", 1, [solvers.ExamSection("S1", ["s1_only"], ["R0"])], [0, 9 * DAY]),
        solvers.Exam("E2", 1, [solvers.ExamSection("S2", ["s02"], ["R0"])], [9 * DAY]),
        solvers.Exam("E3", 10, [solvers.ExamSection("S3", ["s03"], ["R0"])], [DAY]),
    ]
    problem_conflict = solvers.ProblemData(exams_conflict, [solvers.Room("R0", 10)])
    ls = solvers.LsParams()
    ls.prob_1_move = 0.0
    ls.max_improvements = 10
    ls.patience = 200
    eval_p = solvers.EvalParams()

    # Slot ordering: 0 -> 0, 1 -> DAY, 2 -> 9*DAY. E0 at 0, E1 at 2, E2 at 2, E3 at 1.
    res_conflict = solvers.improve_slots(problem_conflict, [0, 2, 2, 1], ls, eval_p, base_seed=42)
    assert res_conflict.schedule == [0, 2, 2, 1]
    assert_valid_schedule(problem_conflict, exams_conflict, res_conflict.schedule)

    # - Case B: E1 is only feasible at slot 2, blocking E1 -> slot 0.
    exams_infeasible = [
        solvers.Exam("E0", 10, [solvers.ExamSection("S0", ["s03"], ["R0"])], [0, 9 * DAY]),
        solvers.Exam("E1", 1, [solvers.ExamSection("S1", ["s1_only"], ["R0"])], [9 * DAY]),
        solvers.Exam("E3", 10, [solvers.ExamSection("S3", ["s03"], ["R0"])], [DAY]),
    ]
    problem_infeasible = solvers.ProblemData(exams_infeasible, [solvers.Room("R0", 10)])
    res_infeasible = solvers.improve_slots(problem_infeasible, [0, 2, 1], ls, eval_p, base_seed=42)
    assert res_infeasible.schedule == [0, 2, 1]
    assert_valid_schedule(problem_infeasible, exams_infeasible, res_infeasible.schedule)


def test_local_search_respects_budget_limits_and_single_exam_swap_edge_case():
    # Two independent 1-move improvements exist; max_improvements=1 must apply at most one.
    exams = [
        solvers.Exam("E0", 3, [solvers.ExamSection("S0", ["a"], ["R0"])], [0]),
        solvers.Exam("E1", 3, [solvers.ExamSection("S1", ["a"], ["R0"])], [DAY, 10 * DAY]),
        solvers.Exam("E2", 3, [solvers.ExamSection("S2", ["b"], ["R0"])], [0]),
        solvers.Exam("E3", 3, [solvers.ExamSection("S3", ["b"], ["R0"])], [DAY, 10 * DAY]),
    ]
    problem = solvers.ProblemData(exams, [solvers.Room("R0", 10)])
    eval_p = solvers.EvalParams()

    ls_zero = solvers.LsParams()
    ls_zero.max_improvements = 0
    res_zero = solvers.improve_slots(problem, [0, 1, 0, 1], ls_zero, eval_p, base_seed=1)
    assert res_zero.schedule == [0, 1, 0, 1]
    assert res_zero.penalty == pytest.approx(
        compute_exact_penalty(problem, exams, [0, 1, 0, 1], eval_p.penalty_decay_base)
    )

    ls_one = solvers.LsParams()
    ls_one.prob_1_move = 1.0
    ls_one.max_improvements = 1
    ls_one.patience = 200
    res_one = solvers.improve_slots(problem, [0, 1, 0, 1], ls_one, eval_p, base_seed=1)
    changed = sum(a != b for a, b in zip([0, 1, 0, 1], res_one.schedule))
    assert changed == 1
    assert res_one.penalty == pytest.approx(
        compute_exact_penalty(problem, exams, res_one.schedule, eval_p.penalty_decay_base)
    )

    # Single-exam problem with 2-swap only (exam1 == exam2 == 0) must terminate cleanly.
    single = solvers.ProblemData(
        [solvers.Exam("ONLY", 3, [solvers.ExamSection("S0", ["x"], ["R0"])], [0, DAY])],
        [solvers.Room("R0", 10)],
    )
    ls_swap = solvers.LsParams()
    ls_swap.prob_1_move = 0.0
    ls_swap.patience = 10
    res_single = solvers.improve_slots(single, [0], ls_swap, eval_p, base_seed=1)
    assert res_single.schedule == [0]
    assert res_single.penalty == 0.0
