#include "common/problem_data.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
#include <string_view>
#include <unordered_map>

#include "common/exam.hpp"
#include "common/room.hpp"
#include "utils/assert.hpp"
#include "utils/matrix.hpp"

#ifndef NDEBUG
#include <unordered_set>
#endif  // NDEBUG

namespace common {

ProblemData::ProblemData(const Vector<Exam>& _exams, Vector<int64_t> _slot_timestamps,
                         const Vector<Room>& _rooms)
    : num_exams(_exams.size()),
      num_slots(_slot_timestamps.size()),
      num_rooms(_rooms.size()),
      num_sections(extract_num_sections(_exams)),
      num_enrollments(extract_num_enrollments(_exams)),

      id_to_student(build_id_to_student(_exams, num_enrollments)),
      student_to_id(build_student_to_id(id_to_student)),

      exam_to_sections(build_exam_to_sections(_exams)),
      section_to_exam(build_section_to_exam(_exams, num_sections)),

      exam_codes(extract_exam_codes(_exams)),
      exam_credits(extract_exam_credits(_exams)),
      exam_student_counts(extract_exam_student_counts(_exams)),

      section_codes(extract_section_codes(_exams, num_sections)),
      section_students(
          extract_section_students(_exams, student_to_id, num_sections, num_enrollments)),
      section_student_counts(extract_section_student_counts(_exams, num_sections)),

      slot_timestamps(std::move(_slot_timestamps)),

      room_codes(extract_room_codes(_rooms)),
      room_capacities(extract_room_capacities(_rooms)),
      room_locations(extract_room_locations(_rooms)),
      room_types(extract_room_types(_rooms)),

      conflicts_matrix(build_conflicts_matrix(_exams, student_to_id)),
      conflicts_csrmatrix(conflicts_matrix),
      weighted_conflict_degrees(build_weighted_conflict_degrees(conflicts_csrmatrix)),

      feasible_slots_by_exam(build_feasible_slots_by_exam(_exams, num_slots)),
      feasible_rooms_by_section(build_feasible_rooms_by_section(_exams, num_rooms, num_sections)) {
#ifndef NDEBUG
  PANIC_IF(num_slots == 0, "'_slot_timestamps' is empty");
  PANIC_IF(num_rooms == 0, "'_rooms' is empty");
  std::unordered_set<std::string_view> unique_codes_checker;
  for (const std::string_view code : exam_codes) {
    PANIC_IF(!unique_codes_checker.emplace(code).second, "Duplicate exam code detected: {}", code);
  }
#endif  // NDEBUG
}

auto ProblemData::sections_of_exam(int exam) const -> std::pair<int, int> {
  PANIC_IF(exam < 0 || exam >= num_exams, "Exam index out of bounds: {}", exam);
  const int begin = exam_to_sections[exam];
  const int end = exam == num_exams - 1 ? num_sections : exam_to_sections[exam + 1];
  return {begin, end};
}

auto ProblemData::extract_num_sections(const Vector<Exam>& exams) -> int {
  PANIC_IF(exams.empty(), "'_exams' is empty");
  int num_sections = 0;
  for (const Exam& exam : exams) {
    num_sections += exam.section_count();
  }
  return num_sections;
}

auto ProblemData::extract_num_enrollments(const Vector<Exam>& exams) -> int {
  int num_enrollments = 0;
  for (const Exam& exam : exams) {
    num_enrollments += exam.student_count();
  }
  return num_enrollments;
}

auto ProblemData::build_id_to_student(const Vector<Exam>& exams, int num_enrollments)
    -> Vector<std::string> {
  Vector<std::string_view> temp_views;
  temp_views.reserve(num_enrollments);
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      for (const std::string_view student : section.students()) {
        temp_views.emplace_back(student);
      }
    }
  }
  std::ranges::sort(temp_views);
  auto [trash_begin, trash_end] = std::ranges::unique(temp_views);
  temp_views.erase(trash_begin, trash_end);
  Vector<std::string> id_to_student;
  id_to_student.reserve(temp_views.size());
  for (const auto& view : temp_views) {
    id_to_student.emplace_back(view);
  }
  return id_to_student;
}

auto ProblemData::build_student_to_id(const Vector<std::string>& id_to_student)
    -> std::unordered_map<std::string_view, int> {
  std::unordered_map<std::string_view, int> student_to_id;
  student_to_id.reserve(size_t(id_to_student.size()));
  int current_id = 0;
  for (const std::string_view student : id_to_student) {
    student_to_id[student] = current_id++;
  }
  return student_to_id;
}

auto ProblemData::extract_exam_codes(const Vector<Exam>& exams) -> Vector<std::string> {
  Vector<std::string> codes;
  codes.reserve(exams.size());
  for (const Exam& exam : exams) {
    codes.emplace_back(exam.code());
  }
  return codes;
}

auto ProblemData::extract_exam_credits(const Vector<Exam>& exams) -> Vector<int> {
  const int num_exams = exams.size();
  Vector<int> credits(num_exams);
  for (int i = 0; i < num_exams; ++i) {
    credits[i] = exams[i].credits();
  }
  return credits;
}

auto ProblemData::extract_section_codes(const Vector<Exam>& exams, int num_sections)
    -> Vector<std::string> {
  Vector<std::string> section_codes;
  section_codes.reserve(num_sections);
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      section_codes.emplace_back(section.code());
    }
  }
  return section_codes;
}

auto ProblemData::extract_section_students(
    const Vector<Exam>& exams, const std::unordered_map<std::string_view, int>& student_to_id,
    int num_sections, int num_enrollments)  // NOLINT(bugprone-easily-swappable-parameters)
    -> CsrMatrix<int> {
  Vector<Vector<int>> section_students;
  section_students.reserve(num_sections);
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      Vector<int> students;
      students.reserve(section.student_count());
      for (const std::string_view student : section.students()) {
        students.emplace_back(student_to_id.at(student));
      }
      section_students.emplace_back(std::move(students));
    }
  }
  return {section_students, num_enrollments, false};
}

auto ProblemData::extract_section_student_counts(const Vector<Exam>& exams, int num_sections)
    -> Vector<int> {
  Vector<int> sections_student_counts(num_sections);
  int section_index = 0;
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      sections_student_counts[section_index++] = section.student_count();
    }
  }
  return sections_student_counts;
}

auto ProblemData::extract_exam_student_counts(const Vector<Exam>& exams) -> Vector<int> {
  const int num_exams = exams.size();
  Vector<int> exam_student_counts(num_exams);
  for (int i = 0; i < num_exams; ++i) {
    exam_student_counts[i] = exams[i].student_count();
  }
  return exam_student_counts;
}

auto ProblemData::build_exam_to_sections(const Vector<Exam>& exams) -> Vector<int> {
  const int num_exams = exams.size();
  Vector<int> exam_to_sections(num_exams);
  int count = 0;
  for (int i = 0; i < num_exams; ++i) {
    exam_to_sections[i] = count;
    count += exams[i].section_count();
  }
  return exam_to_sections;
}

auto ProblemData::build_section_to_exam(const Vector<Exam>& exams, int num_sections)
    -> Vector<int> {
  Vector<int> section_to_exam(num_sections);
  int section_index = 0;
  for (int exam_index = 0; exam_index < exams.size(); ++exam_index) {
    const int num_sections_in_exam = exams[exam_index].section_count();
    for (int i = 0; i < num_sections_in_exam; ++i) {
      section_to_exam[section_index++] = exam_index;
    }
  }
  return section_to_exam;
}

auto ProblemData::extract_room_codes(const Vector<Room>& rooms) -> Vector<std::string> {
  Vector<std::string> room_codes;
  room_codes.reserve(rooms.size());
  for (const Room& room : rooms) {
    room_codes.emplace_back(room.code());
  }
  return room_codes;
}

auto ProblemData::extract_room_capacities(const Vector<Room>& rooms) -> Vector<int> {
  const int num_rooms = rooms.size();
  Vector<int> room_capacities(num_rooms);
  for (int i = 0; i < num_rooms; ++i) {
    room_capacities[i] = rooms[i].capacity();
  }
  return room_capacities;
}

auto ProblemData::extract_room_locations(const Vector<Room>& rooms) -> Vector<std::string> {
  Vector<std::string> room_locations;
  room_locations.reserve(rooms.size());
  for (const Room& room : rooms) {
    room_locations.emplace_back(room.location());
  }
  return room_locations;
}

auto ProblemData::extract_room_types(const Vector<Room>& rooms) -> Vector<std::string> {
  Vector<std::string> room_types;
  room_types.reserve(rooms.size());
  for (const Room& room : rooms) {
    room_types.emplace_back(room.type());
  }
  return room_types;
}

auto ProblemData::build_conflicts_matrix(
    const Vector<Exam>& exams, const std::unordered_map<std::string_view, int>& student_to_id)
    -> Matrix<int> {
  Vector<Vector<int>> exams_by_student(student_to_id.size());
  int exam_index = 0;
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      for (const std::string_view student : section.students()) {
        const auto student_id = student_to_id.at(student);
        exams_by_student[student_id].emplace_back(exam_index);
      }
    }
    ++exam_index;
  }
  const int num_exams = exams.size();
  Matrix<int> conflicts_matrix(num_exams, num_exams, 0);
  for (const Vector<int>& student_exams : exams_by_student) {
    const int num_exams_by_student = student_exams.size();
    for (int i = 0; i < num_exams_by_student; ++i) {
      const int exam_i = student_exams[i];
      for (int j = i + 1; j < num_exams_by_student; ++j) {
        const int exam_j = student_exams[j];
        ++conflicts_matrix[exam_i, exam_j];
        ++conflicts_matrix[exam_j, exam_i];
      }
    }
  }
  return conflicts_matrix;
}

auto ProblemData::build_weighted_conflict_degrees(const CsrMatrix<int>& conflicts_csrmatrix)
    -> Vector<int> {
  const int num_exams = conflicts_csrmatrix.num_rows();
  Vector<int> total_conflicts(num_exams);
  for (int exam = 0; exam < num_exams; ++exam) {
    const auto row = conflicts_csrmatrix[exam].values;
    total_conflicts[exam] = std::accumulate(row.begin(), row.end(), 0);
  }
  return total_conflicts;
}

auto ProblemData::build_feasible_slots_by_exam(const Vector<Exam>& exams, int num_slots)
    -> CsrMatrix<int> {
  const int num_exams = exams.size();
  Matrix<int> feasible_slots(num_exams, num_slots, -1);
  int feasible_count = 0;
  int exam_index = 0;
  for (const Exam& exam : exams) {
    const auto& feasible = exam.feasible_slots();
    feasible_count += feasible.size();
    for (int slot : feasible) {
      PANIC_IF(slot < 0 || slot >= num_slots,
               "Exam {}: 'feasible_slots' has value {} out of bounds [0, {}]", exam.code(), slot,
               num_slots - 1);
      feasible_slots[exam_index, slot] = slot;
    }
    ++exam_index;
  }
  return {feasible_slots, feasible_count, false, -1};
}

auto ProblemData::build_feasible_rooms_by_section(const Vector<Exam>& exams, int num_rooms,
                                                  int num_sections) -> CsrMatrix<int> {
  Matrix<int> feasible_rooms(num_sections, num_rooms, -1);
  int feasible_count = 0;
  int section_index = 0;
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      const auto& feasible = section.feasible_rooms();
      feasible_count += feasible.size();
      for (int room : feasible) {
        PANIC_IF(room < 0 || room >= num_rooms,
                 "Section {}: 'feasible_rooms' has value {} out of bounds [0, {}]", section.code(),
                 room, num_rooms - 1);
        feasible_rooms[section_index, room] = room;
      }
      ++section_index;
    }
  }
  return {feasible_rooms, feasible_count, false, -1};
}

}  // namespace common
