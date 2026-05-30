#include "common/problem_data.hpp"
#include "common/exam.hpp"
#include "utils/matrix.hpp"

#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace common {

ProblemData::ProblemData(
  const std::vector<Exam>& _exams,
  std::vector<int64_t> _slot_timestamps,
  const std::vector<Room>& _rooms
):
  num_exams(_exams.size()),
  num_slots(_slot_timestamps.size()),
  num_rooms(_rooms.size()),
  exam_codes(extract_exam_codes(_exams)),
  student_to_id(build_student_to_id(_exams)),
  id_to_student(build_id_to_student(student_to_id)),
  exam_students(extract_exam_students(_exams, student_to_id)),
  exam_credits(extract_exam_credits(_exams)),
  slot_timestamps(std::move(_slot_timestamps)),
  room_codes(extract_room_codes(_rooms)),
  room_capacities(extract_room_capacities(_rooms)),
  room_locations(extract_room_locations(_rooms)),
  room_types(extract_room_types(_rooms)),
  conflicts_matrix(build_conflicts_matrix(exam_students)),
  conflicts_csrmatrix(conflicts_matrix),
  weighted_conflict_degrees(build_weighted_conflict_degrees(conflicts_csrmatrix)),
  feasible_slots(build_feasible_slots(_exams, num_slots)),
  feasible_rooms(build_feasible_rooms(_exams, num_rooms))
{
#ifndef NDEBUG
  PANIC_IF(num_exams == 0, "'_exams' is empty");
  PANIC_IF(num_slots == 0, "'_slot_timestamps' is empty");
  PANIC_IF(num_rooms == 0, "'_rooms' is empty");
  std::unordered_set<std::string_view> unique_codes_checker;
  for (int i = 0; i < num_exams; ++i) {
    PANIC_IF(
      !unique_codes_checker.emplace(exam_codes[i]).second,
      "Duplicate exam code detected: {}", exam_codes[i]
    );
    for (int slot : feasible_slots[i].values) {
      PANIC_IF(
        slot >= num_slots,
        "Exam {}: 'feasible_slots' has value {} out of bounds [0, {}]",
        exam_codes[i], slot, num_slots - 1
      );
    }
    for (int room : feasible_rooms[i].values) {
      PANIC_IF(
        room >= num_rooms,
        "Exam {}: 'feasible_rooms' has value {} out of bounds [0, {}]",
        exam_codes[i], room, num_rooms - 1
      );
    }
  }
#endif // NDEBUG
}

std::vector<std::string> ProblemData::extract_exam_codes(const std::vector<Exam>& exams) {
  std::vector<std::string> codes(exams.size());
  for (int i = 0; i < exams.size(); ++i) {
    codes[i] = exams[i].code();
  }
  return codes;
}

std::unordered_map<std::string, int> ProblemData::build_student_to_id(const std::vector<Exam>& exams) {
  std::unordered_map<std::string, int> student_to_id;
  int count = 0;
  for (const Exam& exam : exams) {
    count += exam.students().size();
  }
  student_to_id.reserve(count);
  for (const Exam& exam : exams) {
    for (const std::string& student : exam.students()) {
      if (!student_to_id.contains(student)) {
        student_to_id.emplace(student, student_to_id.size());
      }
    }
  }
  return student_to_id;
}

std::vector<std::string> ProblemData::build_id_to_student(const std::unordered_map<std::string, int>& student_to_id) {
  std::vector<std::string> id_to_student(student_to_id.size());
  for (const auto& [student, id] : student_to_id) {
    id_to_student[id] = student;
  }
  return id_to_student;
}

utils::CsrMatrix<int> ProblemData::extract_exam_students(
  const std::vector<Exam>& exams,
  const std::unordered_map<std::string, int>& student_to_id
) {
  std::vector<std::vector<int>> exam_students(exams.size());
  int count = 0;
  for (int i = 0; i < exams.size(); ++i) {
    for (const std::string& student : exams[i].students()) {
      exam_students[i].emplace_back(student_to_id.at(student));
      ++count;
    }
  }
  return utils::CsrMatrix<int>(exam_students, count, false);
}

std::vector<int> ProblemData::extract_exam_credits(const std::vector<Exam>& exams) {
  std::vector<int> credits(exams.size());
  for (int i = 0; i < exams.size(); ++i) {
    credits[i] = exams[i].credits();
  }
  return credits;
}

std::vector<std::string> ProblemData::extract_room_codes(const std::vector<Room>& rooms) {
  std::vector<std::string> room_codes(rooms.size());
  for (int i = 0; i < rooms.size(); ++i) {
    room_codes[i] = rooms[i].code();
  }
  return room_codes;
}

std::vector<int> ProblemData::extract_room_capacities(const std::vector<Room>& rooms) {
  std::vector<int> room_capacities(rooms.size());
  for (int i = 0; i < rooms.size(); ++i) {
    room_capacities[i] = rooms[i].capacity();
  }
  return room_capacities;
}

std::vector<std::string> ProblemData::extract_room_locations(const std::vector<Room>& rooms) {
  std::vector<std::string> room_locations(rooms.size());
  for (int i = 0; i < rooms.size(); ++i) {
    room_locations[i] = rooms[i].location();
  }
  return room_locations;
}

std::vector<std::string> ProblemData::extract_room_types(const std::vector<Room>& rooms) {
  std::vector<std::string> room_types(rooms.size());
  for (int i = 0; i < rooms.size(); ++i) {
    room_types[i] = rooms[i].type();
  }
  return room_types;
}

utils::Matrix<int> ProblemData::build_conflicts_matrix(const utils::CsrMatrix<int>& exam_students) {
  const int num_exams = exam_students.num_rows();
  utils::Matrix<int> conflicts_matrix(num_exams, num_exams, 0);
  for (int i = 0; i < num_exams; ++i) {
    std::unordered_set<int> students_i;
    students_i.reserve(exam_students[i].size());
    for (int student_i : exam_students[i].values) {
      students_i.emplace(student_i);
    }
    for (int j = i + 1; j < num_exams; ++j) {
      int common_count = 0;
      for (int student_j : exam_students[j].values) {
        common_count += students_i.contains(student_j);
      }
      conflicts_matrix(i, j) = conflicts_matrix(j, i) = common_count;
    }
  }
  return conflicts_matrix;
}

std::vector<int> ProblemData::build_weighted_conflict_degrees(const utils::CsrMatrix<int>& conflicts_csrmatrix) {
  const int num_exams = conflicts_csrmatrix.num_rows();
  std::vector<int> total_conflicts(num_exams);
  for (int exam = 0; exam < num_exams; ++exam) {
    const auto row = conflicts_csrmatrix[exam].values;
    total_conflicts[exam] = std::accumulate(row.begin(), row.end(), 0);
  }
  return total_conflicts;
}

utils::CsrMatrix<int> ProblemData::build_feasible_slots(const std::vector<Exam>& exams, int num_slots) {
  utils::Matrix<int> feasible_slots(exams.size(), num_slots, -1);
  int count = 0;
  for (int i = 0; i < exams.size(); ++i) {
    count += exams[i].feasible_slots().size();
    for (int slot : exams[i].feasible_slots()) {
      feasible_slots(i, slot) = slot;
    }
  }
  return utils::CsrMatrix<int>(feasible_slots, count, false, -1);
}

utils::CsrMatrix<int> ProblemData::build_feasible_rooms(const std::vector<Exam>& exams, int num_rooms) {
  utils::Matrix<int> feasible_rooms(exams.size(), num_rooms, -1);
  int count = 0;
  for (int i = 0; i < exams.size(); ++i) {
    count += exams[i].feasible_rooms().size();
    for (int room : exams[i].feasible_rooms()) {
      feasible_rooms(i, room) = room;
    }
  }
  return utils::CsrMatrix<int>(feasible_rooms, count, false, -1);
}

} // namespace common