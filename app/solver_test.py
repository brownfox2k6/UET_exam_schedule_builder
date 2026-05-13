import pytest
import solver  # Import qua file __init__.py trong thư mục solver/

def test_hyperparams_initialization():
    """Kiểm tra xem cầu nối C++ <-> Python có gán đúng Hyperparameters không"""
    hp = solver.Hyperparams()
    
    # Gán thử giá trị
    hp.aco.num_iters = 50
    hp.aco.num_ants = 15
    hp.ls.prob_1_move = 0.8
    
    # Đảm bảo C++ nhận đúng giá trị
    assert hp.aco.num_iters == 50
    assert hp.aco.num_ants == 15
    assert hp.ls.prob_1_move == 0.8

@pytest.fixture
def sample_data():
    """Tạo dữ liệu giả lập cho bài toán xếp lịch thi (4 môn, 3 slot)"""
    num_exams = 4
    num_slots = 3
    
    # Ma trận xung đột sinh viên (Đối xứng, đường chéo = 0)
    conflicts = [
        [0, 10,  0,  0],
        [10, 0,  5,  0],
        [0,  5,  0,  2],
        [0,  0,  2,  0]
    ]
    
    # Timestamps (mỗi slot cách nhau 1 ngày = 86400 giây)
    timestamps = [0, 86400, 172800]
    
    hp = solver.Hyperparams()
    hp.aco.num_ants = 5
    hp.aco.num_iters = 10  # Chạy test nhanh với 10 iters
    
    # Vẫn trả về num_exams và num_slots để dùng cho các hàm assert kiểm tra kết quả
    return num_exams, num_slots, hp, conflicts, timestamps

def test_ant_colony_run_one_iteration(sample_data):
    """Kiểm tra hàm chạy 1 vòng lặp"""
    num_exams, num_slots, hp, conflicts, timestamps = sample_data
    base_seed = 42
    
    # Đã sửa: Khởi tạo AntColony chỉ với 4 tham số theo đúng yêu cầu của C++
    colony = solver.AntColony(hp, conflicts, timestamps, base_seed)
    
    # Chạy 1 vòng
    best_cost = colony.run_one_iteration()
    
    assert isinstance(best_cost, float)
    assert best_cost >= 0.0, "Fitness (số lượng vi phạm) không thể là số âm"

def test_ant_colony_full_run_and_callback(sample_data):
    """Kiểm tra hàm chạy toàn bộ thuật toán và cơ chế Callback về Python"""
    num_exams, num_slots, hp, conflicts, timestamps = sample_data
    
    # Đã sửa: Khởi tạo AntColony chỉ với 4 tham số
    colony = solver.AntColony(hp, conflicts, timestamps, 123)
    
    # Biến để hứng dữ liệu từ C++ callback
    callback_history = []
    
    def monitor_progress(iteration, current_best_cost):
        callback_history.append((iteration, current_best_cost))
        
    # Chạy toàn bộ thuật toán
    colony.run(monitor_progress)
    
    # Kiểm tra callback có được gọi đủ số lần không
    assert len(callback_history) == hp.aco.num_iters
    
    # Kiểm tra kết quả con kiến tốt nhất (global_best)
    best_schedule = colony.global_best_schedule
    best_fitness = colony.global_best_fitness
    
    assert len(best_schedule) == num_exams, "Thiếu lịch thi của một số môn"
    
    # Kiểm tra xem có môn nào bị bỏ sót (-1) không và slot có hợp lệ không
    for exam_id, assigned_slot in enumerate(best_schedule):
        assert 0 <= assigned_slot < num_slots, f"Môn {exam_id} được xếp vào slot không hợp lệ: {assigned_slot}"
        
    assert isinstance(best_fitness, float)