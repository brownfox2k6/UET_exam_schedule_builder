"""
GOOGLE GEMINI GENERATED
Pretests for solver
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
# 1. FIXTURES - KHỞI TẠO DỮ LIỆU MẪU
# ==========================================

@pytest.fixture
def sample_data():
    """Tạo dữ liệu giả lập chuẩn theo cấu trúc thực tế (4 môn thi, 3 ca thi)"""
    hp = solver.Hyperparams()
    hp.aco.num_ants = 4
    hp.aco.num_iters = 5  # Giữ số vòng lặp nhỏ để test chạy cực nhanh
    
    # Giả lập mốc thời gian Unix Epoch (mỗi slot cách nhau 1 ngày = 86400s)
    timestamps = [1716613200, 1716699600, 1716786000]
    
    # Thiết lập danh sách môn thi (Exam) có ràng buộc chồng lấn sinh viên và tín chỉ
    # Cú pháp C++: Exam(code, credits, students, feasible_slots, feasible_rooms, feasible_proctors)
    exams = [
        solver.Exam("MATH101", 3, ["SV01", "SV02", "SV03"], [0, 1, 2], [101, 102], [201]),
        solver.Exam("PHYS101", 4, ["SV02", "SV03", "SV04"], [0, 1, 2], [101, 102], [201]),
        solver.Exam("PROG101", 2, ["SV04", "SV05"],         [0, 1],    [101],      [202]),
        solver.Exam("ENGL101", 1, ["SV06"],                 [1, 2],    [102],      [202])
    ]
    return exams, timestamps, hp


# ==========================================
# 2. UNIT TESTS - KIỂM TRA TỪNG THÀNH PHẦN
# ==========================================

def test_exam_initialization_and_properties():
    """TEST 1: Kiểm tra việc tạo và đọc ghi thuộc tính của class Exam"""
    exam = solver.Exam("CS101", 3, ["S1", "S2"], [0, 1], [101], [201])
    
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
    
    # Thay đổi các thông số ở cả 3 nhóm tham số khác nhau
    hp.eval.penalty_decay_base = 2.5
    hp.aco.alpha = 1.2
    hp.aco.beta = 1.8
    hp.ls.patience = 50
    
    # Đảm bảo C++ lưu giữ chính xác
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
    
    # Khởi tạo đàn kiến, kiểm tra xem có bị crash bộ nhớ hay văng lỗi loại dữ liệu không
    colony = solver.AntColony(hp, exams, timestamps, base_seed=42)
    
    # Sau khi truyền vào Colony, danh sách ban đầu ở Python vẫn phải toàn vẹn cấu trúc
    assert len(exams) == 4
    assert colony.global_best_fitness > 0.0


# ==========================================
# 3. INTEGRATION TESTS - KIỂM TRA CORE THUẬT TOÁN
# ==========================================

def test_ant_colony_run_one_iteration(sample_data):
    """TEST 5: Kiểm tra tính đúng đắn của 1 bước tiến hóa (Dựng lịch + Local Search)"""
    exams, timestamps, hp = sample_data
    colony = solver.AntColony(hp, exams, timestamps, base_seed=100)
    
    cost = colony.run_one_iteration()
    
    assert isinstance(cost, float)
    assert cost >= 0.0, "Điểm phạt (Penalty/Fitness) không thể âm"


def test_ant_colony_full_run_with_callback(sample_data):
    """TEST 6: Kiểm tra hàm run() tổng và cơ chế thu hồi GIL để tương tác Callback về Python"""
    exams, timestamps, hp = sample_data
    colony = solver.AntColony(hp, exams, timestamps, base_seed=999)
    
    history = []
    def python_callback(iteration, best_cost):
        history.append((iteration, best_cost))
        
    # Chạy full thuật toán
    colony.run(python_callback)
    
    # Hệ thống phải trigger callback đúng số vòng lặp cấu hình
    assert len(history) == hp.aco.num_iters
    assert history[0][0] == 1  # Vòng lặp đầu tiên bắt đầu từ 1
    assert history[-1][0] == hp.aco.num_iters


def test_solution_schedule_integrity(sample_data):
    """TEST 7: Kiểm tra tính hợp lệ của mảng kết quả lịch thi sau khi tối ưu xong"""
    exams, timestamps, hp = sample_data
    colony = solver.AntColony(hp, exams, timestamps, base_seed=777)
    
    colony.run()  # Chạy không cần truyền callback (Test tính năng Safe-Null)
    
    best_schedule = colony.global_best_schedule
    
    assert len(best_schedule) == len(exams), "Số lượng môn được xếp lịch bị lệch"
    for slot_assigned in best_schedule:
        # Ca thi được phân phối phải nằm trong phạm vi chỉ mục hợp lệ của mảng timestamps
        assert 0 <= slot_assigned < len(timestamps)


def test_solver_determinism_with_same_seed(sample_data):
    """TEST 8: Kiểm tra tính nhất quán (Determinism). Cùng seed phải ra kết quả y hệt nhau"""
    exams, timestamps, hp = sample_data
    
    colony_a = solver.AntColony(hp, exams, timestamps, base_seed=12345)
    colony_b = solver.AntColony(hp, exams, timestamps, base_seed=12345)
    
    cost_a = colony_a.run_one_iteration()
    cost_b = colony_b.run_one_iteration()
    
    assert cost_a == cost_b, "Thuật toán bị mất tính nhất quán dù chung hạt giống PRNG Seed"
    assert colony_a.global_best_schedule == colony_b.global_best_schedule


def test_solver_stochasticity_with_different_seeds(sample_data):
    """TEST 9: Kiểm tra tính ngẫu nhiên (Stochasticity). Khác seed phải sinh ra đường đi của kiến khác nhau"""
    exams, timestamps, hp = sample_data
    
    # Tăng số kiến lên một chút để thấy rõ sự khác biệt giữa 2 thực thể đàn kiến độc lập
    hp.aco.num_ants = 20
    
    colony_a = solver.AntColony(hp, exams, timestamps, base_seed=111)
    colony_b = solver.AntColony(hp, exams, timestamps, base_seed=222)
    
    colony_a.run_one_iteration()
    colony_b.run_one_iteration()
    
    # Xác suất cực cao (gần như 100%) là hai đàn kiến độc lập với hạt giống khác nhau 
    # sẽ có cấu trúc tìm kiếm giải pháp ban đầu không trùng khít hoàn toàn
    assert colony_a.global_best_fitness != colony_b.global_best_fitness or \
           colony_a.global_best_schedule != colony_b.global_best_schedule
