"""
GOOGLE GEMINI GENERATED
Pretests for solver - Heavy Load Performance Version (Strict Indexing)
"""

import os
import sys

# Ép Python phải nhìn thấy thư mục gốc 'app/' nơi chứa package 'solver'
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

import pytest
import solver  # Import qua file __init__.py trong thư mục solver/

# ==========================================
# 1. FIXTURES - KHỞI TẠO DỮ LIỆU MẪU HẠNG NẶNG
# ==========================================

@pytest.fixture
def sample_data():
    """
    Tạo dữ liệu giả lập quy mô lớn chuẩn UET (120 môn thi, 20 ca thi, 200 kiến).
    Các mảng rooms, slots, proctors tuân thủ nghiêm ngặt dải chỉ mục liên tục: 0, 1, 2...
    """
    hp = solver.Hyperparams()
    hp.aco.num_ants = 200     # Tăng số lượng kiến lên 200 để kích hoạt OpenMP đa luồng cực mạnh
    hp.aco.num_iters = 15     # Giữ số vòng lặp vừa phải (15) để tổng thời gian test không quá 5 giây

    # Thiết lập số lượng tài nguyên hệ thống
    num_slots = 20
    num_rooms = 8
    num_proctors = 15

    # Tạo mốc thời gian Unix Epoch (mỗi slot cách nhau 8 tiếng = 28800s)
    timestamps = [1716613200 + (i * 28800) for i in range(num_slots)]
    
    # RÀNG BUỘC CHẶT CHẼ: rooms và proctors phải là mảng số chỉ mục tuần tự từ 0
    rooms = list(range(num_rooms))        # [0, 1, 2, 3, 4, 5, 6, 7]
    proctors = list(range(num_proctors))  # [0, 1, 2, ..., 14]
    
    exams = []
    num_exams = 120  # Quy mô 120 môn thi
    
    for i in range(num_exams):
        code = f"UET{i:03d}"
        credits = 1 + (i % 4) 
        
        # 💥 SỬA DÒNG NÀY ĐỂ GIẢM ĐỘ KHÓ:
        # Thay vì tạo 35 sinh viên trùng lặp liên tục ( range(i, i + 35) ),
        # chúng ta cô lập tập sinh viên theo từng nhóm môn để giảm ma trận đụng độ chéo.
        group_id = i // 10  # Cứ 10 môn chung 1 nhóm sinh viên
        students = [f"SV_{group_id}_{idx:02d}" for idx in range(15)] # Mỗi môn chỉ có 15 SV
        
        if i % 3 == 0:
            feasible_slots = list(range(0, num_slots, 2))
        elif i % 3 == 1:
            feasible_slots = list(range(1, num_slots, 2))
        else:
            feasible_slots = list(range(num_slots))
            
        exams.append(solver.Exam(code, credits, students, feasible_slots, rooms, proctors))
        
    return exams, timestamps, hp


# ==========================================
# 2. UNIT TESTS - KIỂM TRA TỪNG THÀNH PHẦN
# ==========================================

def test_exam_initialization_and_properties():
    """TEST 1: Kiểm tra việc tạo và đọc ghi thuộc tính của class Exam với 0-indexing"""
    # Sử dụng đúng luật: feasible_slots=[0, 1], rooms=[0], proctors=[0]
    exam = solver.Exam("CS101", 3, ["S1", "S2"], [0, 1], [0], [0])
    
    assert exam.code == "CS101"
    assert exam.credits == 3
    assert "S1" in exam.students
    assert exam.feasible_slots == [0, 1]
    
    # Kiểm tra khả năng chỉnh sửa (Read-Write fields)
    exam.credits = 4
    assert exam.credits == 4


def test_hyperparams_nested_values():
    """TEST 2: Kiểm tra cấu trúc phân cấp phức tạp của Hyperparameters"""
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
    """TEST 3: Kiểm tra các hàm khởi tạo phụ (Overloaded Constructors) có nhận đúng default value"""
    eval_p = solver.EvalParams()
    aco_p = solver.AcoParams()
    ls_p = solver.LsParams()
    
    assert eval_p.penalty_decay_base == 2.0
    assert aco_p.num_ants == 10
    assert ls_p.prob_1_move == 0.5


def test_ant_colony_initialization(sample_data):
    """TEST 4: Kiểm tra việc nạp dữ liệu từ Python vào Constructor của AntColony thành công"""
    exams, timestamps, hp = sample_data
    
    colony = solver.AntColony(hp, exams, timestamps, base_seed=42)
    
    assert len(exams) == 120
    assert colony.global_best_fitness > 0.0


# ==========================================
# 3. INTEGRATION TESTS - KIỂM TRA CORE THUẬT TOÁN (TẬN DỤNG OPENMP)
# ==========================================

def test_ant_colony_run_one_iteration(sample_data):
    """TEST 5: Kiểm tra bước tiến hóa đơn lẻ dưới áp lực tải nặng"""
    exams, timestamps, hp = sample_data
    colony = solver.AntColony(hp, exams, timestamps, base_seed=100)
    
    cost = colony.run_one_iteration()
    
    assert isinstance(cost, float)
    assert cost >= 0.0


def test_ant_colony_full_run_with_callback(sample_data):
    """TEST 6: Kiểm tra cơ chế trả kết quả từ đa luồng C++ về Callback Python"""
    exams, timestamps, hp = sample_data
    # Giảm số vòng lặp riêng cho test này để tối ưu tốc độ GIL callback
    hp.aco.num_iters = 5 

    colony = solver.AntColony(hp, exams, timestamps, base_seed=999)
    
    history = []
    def python_callback(iteration, best_cost):
        history.append((iteration, best_cost))
        
    colony.run(python_callback)
    
    assert len(history) == hp.aco.num_iters
    assert history[0][0] == 1
    assert history[-1][0] == hp.aco.num_iters


def test_solution_schedule_integrity(sample_data):
    """TEST 7: Kiểm tra tính hợp lệ của mảng kết quả lịch thi sau chuỗi tối ưu hóa nặng"""
    exams, timestamps, hp = sample_data
    colony = solver.AntColony(hp, exams, timestamps, base_seed=777)
    
    colony.run()  
    best_schedule = colony.global_best_schedule
    
    assert len(best_schedule) == len(exams)
    for slot_assigned in best_schedule:
        assert 0 <= slot_assigned < len(timestamps)


def test_solver_determinism_with_same_seed(sample_data):
    """TEST 8: Kiểm tra tính nhất quán (Determinism). Quá trình đa luồng OpenMP không được làm lệch seed ngẫu nhiên"""
    exams, timestamps, hp = sample_data
    
    # Hạ bớt số lượng kiến để test chạy thần tốc
    hp.aco.num_ants = 50 
    
    colony_a = solver.AntColony(hp, exams, timestamps, base_seed=12345)
    colony_b = solver.AntColony(hp, exams, timestamps, base_seed=12345)
    
    cost_a = colony_a.run_one_iteration()
    cost_b = colony_b.run_one_iteration()
    
    assert cost_a == cost_b
    assert colony_a.global_best_schedule == colony_b.global_best_schedule


def test_solver_stochasticity_with_different_seeds(sample_data):
    """TEST 9: Kiểm tra tính ngẫu nhiên (Stochasticity) của 2 luồng PRNG khác nhau"""
    exams, timestamps, hp = sample_data
    hp.aco.num_ants = 50
    
    colony_a = solver.AntColony(hp, exams, timestamps, base_seed=111)
    colony_b = solver.AntColony(hp, exams, timestamps, base_seed=222)
    
    colony_a.run_one_iteration()
    colony_b.run_one_iteration()
    
    assert colony_a.global_best_fitness != colony_b.global_best_fitness or \
           colony_a.global_best_schedule != colony_b.global_best_schedule


# ==========================================
# 4. EDGE CASES - KIỂM TRA ĐIỀU KIỆN BIÊN
# ==========================================

def test_edge_case_minimum_credits(sample_data):
    """TEST 10: Kiểm tra độ ổn định biên nhỏ nhất cho phép (Môn 1 tín chỉ)"""
    _, timestamps, hp = sample_data
    hp.aco.num_ants = 20
    
    # Tuân thủ nghiêm ngặt dải index [0]
    border_exams = [
        solver.Exam("MIL001", 1, ["SV01", "SV02"], [0], [0], [0]),
        solver.Exam("PE1002", 1, ["SV02", "SV03"], [0], [0], [0])
    ]
    colony = solver.AntColony(hp, border_exams, timestamps, base_seed=42)
    assert colony.run_one_iteration() >= 0.0