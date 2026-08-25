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

ProblemData::ProblemData(const std::vector<Exam>& _exams, std::vector<int64_t> _slot_timestamps,
                         const std::vector<Room>& _rooms)
    : num_exams(int(_exams.size())),
      num_slots(int(_slot_timestamps.size())),
      num_rooms(int(_rooms.size())),
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

      feasible_slots(build_feasible_slots(_exams, num_slots)),
      feasible_rooms(build_feasible_rooms(_exams, num_rooms)) {
#ifndef NDEBUG
  PANIC_IF(num_slots == 0, "'_slot_timestamps' is empty");
  PANIC_IF(num_rooms == 0, "'_rooms' is empty");
  std::unordered_set<std::string_view> unique_codes_checker;
  for (const std::string_view code : exam_codes) {
    PANIC_IF(!unique_codes_checker.emplace(code).second, "Duplicate exam code detected: {}", code);
  }
#endif  // NDEBUG
}

auto ProblemData::extract_num_sections(const std::vector<Exam>& exams) -> int {
  PANIC_IF(exams.empty(), "'_exams' is empty");
  int num_sections = 0;
  for (const Exam& exam : exams) {
    num_sections += exam.section_count();
  }
  return num_sections;
}

auto ProblemData::extract_num_enrollments(const std::vector<Exam>& exams) -> int {
  int num_enrollments = 0;
  for (const Exam& exam : exams) {
    num_enrollments += exam.student_count();
  }
  return num_enrollments;
}

auto ProblemData::build_id_to_student(const std::vector<Exam>& exams, int num_enrollments)
    -> std::vector<std::string> {
  std::vector<std::string_view> temp_views;
  temp_views.reserve(size_t(num_enrollments));
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
  std::vector<std::string> id_to_student;
  id_to_student.reserve(temp_views.size());
  for (const auto& view : temp_views) {
    id_to_student.emplace_back(view);
  }
  return id_to_student;
}

auto ProblemData::build_student_to_id(const std::vector<std::string>& id_to_student)
    -> std::unordered_map<std::string_view, int> {
  std::unordered_map<std::string_view, int> student_to_id;
  student_to_id.reserve(id_to_student.size());
  int current_id = 0;
  for (const std::string_view student : id_to_student) {
    student_to_id[student] = current_id++;
  }
  return student_to_id;
}

auto ProblemData::extract_exam_codes(const std::vector<Exam>& exams) -> std::vector<std::string> {
  std::vector<std::string> codes;
  codes.reserve(exams.size());
  for (const Exam& exam : exams) {
    codes.emplace_back(exam.code());
  }
  return codes;
}

auto ProblemData::extract_exam_credits(const std::vector<Exam>& exams) -> std::vector<int> {
  const size_t num_exams = exams.size();
  std::vector<int> credits(num_exams);
  for (size_t i = 0; i < num_exams; ++i) {
    credits[i] = exams[i].credits();
  }
  return credits;
}

auto ProblemData::extract_section_codes(const std::vector<Exam>& exams, int num_sections)
    -> std::vector<std::string> {
  std::vector<std::string> section_codes;
  section_codes.reserve(size_t(num_sections));
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      section_codes.emplace_back(section.code());
    }
  }
  return section_codes;
}

auto ProblemData::extract_section_students(
    const std::vector<Exam>& exams, const std::unordered_map<std::string_view, int>& student_to_id,
    int num_sections, int num_enrollments)  // NOLINT(bugprone-easily-swappable-parameters)
    -> utils::CsrMatrix<int> {
  std::vector<std::vector<int>> section_students;
  section_students.reserve(size_t(num_sections));
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      std::vector<int> students;
      students.reserve(size_t(section.student_count()));
      for (const std::string_view student : section.students()) {
        students.emplace_back(student_to_id.at(student));
      }
      section_students.emplace_back(std::move(students));
    }
  }
  return {section_students, num_enrollments, false};
}

auto ProblemData::extract_section_student_counts(const std::vector<Exam>& exams, int num_sections)
    -> std::vector<int> {
  std::vector<int> sections_student_counts((size_t(num_sections)));
  size_t section_index = 0;
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      sections_student_counts[section_index++] = section.student_count();
    }
  }
  return sections_student_counts;
}

auto ProblemData::extract_exam_student_counts(const std::vector<Exam>& exams) -> std::vector<int> {
  const size_t num_exams = exams.size();
  std::vector<int> exam_student_counts(num_exams);
  for (size_t i = 0; i < num_exams; ++i) {
    exam_student_counts[i] = exams[i].student_count();
  }
  return exam_student_counts;
}

auto ProblemData::build_exam_to_sections(const std::vector<Exam>& exams) -> std::vector<int> {
  const size_t num_exams = exams.size();
  std::vector<int> exam_to_sections(num_exams);
  int count = 0;
  for (size_t i = 0; i < num_exams; ++i) {
    exam_to_sections[i] = count;
    count += exams[i].section_count();
  }
  return exam_to_sections;
}

auto ProblemData::build_section_to_exam(const std::vector<Exam>& exams, int num_sections)
    -> std::vector<int> {
  std::vector<int> section_to_exam((size_t(num_sections)));
  size_t section_index = 0;
  for (size_t exam_index = 0; exam_index < exams.size(); ++exam_index) {
    const int num_sections_in_exam = exams[exam_index].section_count();
    for (int i = 0; i < num_sections_in_exam; ++i) {
      section_to_exam[section_index++] = int(exam_index);
    }
  }
  return section_to_exam;
}

auto ProblemData::extract_room_codes(const std::vector<Room>& rooms) -> std::vector<std::string> {
  std::vector<std::string> room_codes;
  room_codes.reserve(rooms.size());
  for (const Room& room : rooms) {
    room_codes.emplace_back(room.code());
  }
  return room_codes;
}

auto ProblemData::extract_room_capacities(const std::vector<Room>& rooms) -> std::vector<int> {
  const size_t num_rooms = rooms.size();
  std::vector<int> room_capacities(num_rooms);
  for (size_t i = 0; i < num_rooms; ++i) {
    room_capacities[i] = rooms[i].capacity();
  }
  return room_capacities;
}

auto ProblemData::extract_room_locations(const std::vector<Room>& rooms)
    -> std::vector<std::string> {
  std::vector<std::string> room_locations;
  room_locations.reserve(rooms.size());
  for (const Room& room : rooms) {
    room_locations.emplace_back(room.location());
  }
  return room_locations;
}

auto ProblemData::extract_room_types(const std::vector<Room>& rooms) -> std::vector<std::string> {
  std::vector<std::string> room_types;
  room_types.reserve(rooms.size());
  for (const Room& room : rooms) {
    room_types.emplace_back(room.type());
  }
  return room_types;
}

auto ProblemData::build_conflicts_matrix(
    const std::vector<Exam>& exams, const std::unordered_map<std::string_view, int>& student_to_id)
    -> utils::Matrix<int> {
  std::vector<std::vector<int>> exams_by_student(student_to_id.size());
  int exam_index = 0;
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      for (const std::string_view student : section.students()) {
        const auto student_id = size_t(student_to_id.at(student));
        exams_by_student[student_id].emplace_back(exam_index);
      }
    }
    ++exam_index;
  }
  const size_t num_exams = exams.size();
  utils::Matrix<int> conflicts_matrix(num_exams, num_exams, 0);
  for (const std::vector<int>& student_exams : exams_by_student) {
    const size_t num_exams_by_student = student_exams.size();
    for (size_t i = 0; i < num_exams_by_student; ++i) {
      const int exam_i = student_exams[i];
      for (size_t j = i + 1; j < num_exams_by_student; ++j) {
        const int exam_j = student_exams[j];
        ++conflicts_matrix[exam_i, exam_j];
        ++conflicts_matrix[exam_j, exam_i];
      }
    }
  }
  return conflicts_matrix;
}

auto ProblemData::build_weighted_conflict_degrees(const utils::CsrMatrix<int>& conflicts_csrmatrix)
    -> std::vector<int> {
  const auto num_exams = size_t(conflicts_csrmatrix.num_rows());
  std::vector<int> total_conflicts(num_exams);
  for (size_t exam = 0; exam < num_exams; ++exam) {
    const auto row = conflicts_csrmatrix[exam].values;
    total_conflicts[exam] = std::accumulate(row.begin(), row.end(), 0);
  }
  return total_conflicts;
}

auto ProblemData::build_feasible_slots(const std::vector<Exam>& exams, int num_slots)
    -> utils::CsrMatrix<int> {
  const size_t num_exams = exams.size();
  utils::Matrix<int> feasible_slots(num_exams, num_slots, -1);
  size_t feasible_count = 0;
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

auto ProblemData::build_feasible_rooms(const std::vector<Exam>& exams, int num_rooms)
    -> utils::CsrMatrix<int> {
  const size_t num_exams = exams.size();
  utils::Matrix<int> feasible_rooms(num_exams, num_rooms, -1);
  size_t feasible_count = 0;
  int exam_index = 0;
  for (const Exam& exam : exams) {
    const auto& feasible = exam.feasible_rooms();
    feasible_count += feasible.size();
    for (int room : feasible) {
      PANIC_IF(room < 0 || room >= num_rooms,
               "Exam {}: 'feasible_rooms' has value {} out of bounds [0, {}]", exam.code(), room,
               num_rooms - 1);
      feasible_rooms[exam_index, room] = room;
    }
    ++exam_index;
  }
  return {feasible_rooms, feasible_count, false, -1};
}

}  // namespace common