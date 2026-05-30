#include "common/problem_data.hpp"
#include "common/exam.hpp"
#include "common/room.hpp"
#include "utils/assert.hpp"
#include "utils/matrix.hpp"

#include <numeric>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace common {

ProblemData::ProblemData(
  std::vector<Exam> _exams,
  std::vector<int64_t> _slot_timestamps,
  std::vector<Room> _rooms
):
  num_exams(_exams.size()),
  num_slots(_slot_timestamps.size()),
  num_rooms(_rooms.size()),
  num_sections(extract_num_sections(_exams)),
  num_enrolments(extract_num_enrolments(_exams)),

  student_to_id(build_student_to_id(_exams, num_enrolments)),
  id_to_student(build_id_to_student(student_to_id)),

  exam_to_sections(build_exam_to_sections(_exams)),
  section_to_exam(build_section_to_exam(exam_to_sections)),

  exam_codes(extract_exam_codes(_exams)),
  exam_credits(extract_exam_credits(_exams)),
  exam_student_counts(extract_exam_student_counts(_exams)),

  section_codes(extract_section_codes(_exams, num_sections)),
  section_students(extract_section_students(_exams, student_to_id, num_sections, num_enrolments)),
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
  }
#endif // NDEBUG
}

int ProblemData::extract_num_sections(const std::vector<Exam>& exams) {
  int num_sections = 0;
  for (const Exam& exam : exams) {
    num_sections += exam.section_count();
  }
  return num_sections;
}

int ProblemData::extract_num_enrolments(const std::vector<Exam>& exams) {
  int num_enrolments = 0;
  for (const Exam& exam : exams) {
    num_enrolments += exam.student_count();
  }
  return num_enrolments;
}

std::unordered_map<std::string, int> ProblemData::build_student_to_id(const std::vector<Exam>& exams, int num_enrolments) {
  std::unordered_map<std::string, int> student_to_id;
  student_to_id.reserve(num_enrolments);
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      for (const std::string& student : section.students()) {
        student_to_id.try_emplace(student, student_to_id.size());
      }
    }
  }
  student_to_id.rehash(0);
  return student_to_id;
}

std::vector<std::string> ProblemData::build_id_to_student(const std::unordered_map<std::string, int>& student_to_id) {
  std::vector<std::string> id_to_student(student_to_id.size());
  for (const auto& [student, id] : student_to_id) {
    id_to_student[id] = student;
  }
  return id_to_student;
}

std::vector<std::string> ProblemData::extract_exam_codes(const std::vector<Exam>& exams) {
  std::vector<std::string> codes(exams.size());
  for (int i = 0; i < exams.size(); ++i) {
    codes[i] = exams[i].code();
  }
  return codes;
}

std::vector<int> ProblemData::extract_exam_credits(const std::vector<Exam>& exams) {
  std::vector<int> credits(exams.size());
  for (int i = 0; i < exams.size(); ++i) {
    credits[i] = exams[i].credits();
  }
  return credits;
}

std::vector<std::string> ProblemData::extract_section_codes(const std::vector<Exam>& exams, int num_sections) {
  std::vector<std::string> section_codes;
  section_codes.reserve(num_sections);
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      section_codes.emplace_back(section.code());
    }
  }
  return section_codes;
}

utils::CsrMatrix<int> ProblemData::extract_section_students(
  const std::vector<Exam>& exams,
  const std::unordered_map<std::string, int>& student_to_id,
  int num_sections,
  int num_enrolments
) {
  std::vector<std::vector<int>> section_students;
  section_students.reserve(num_sections);
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      std::vector<int> v;
      v.reserve(section.student_count());
      for (const std::string& student : section.students()) {
        v.emplace_back(student_to_id.at(student));
      }
      section_students.emplace_back(std::move(v));
    }
  }
  return utils::CsrMatrix<int>(section_students, num_enrolments, false);
}

std::vector<int> ProblemData::extract_section_student_counts(const std::vector<Exam>& exams, int num_sections) {
  std::vector<int> sections_student_counts;
  sections_student_counts.reserve(num_sections);
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections()) {
      sections_student_counts.emplace_back(section.student_count());
    }
  }
  return sections_student_counts;
}

std::vector<int> ProblemData::extract_exam_student_counts(const std::vector<Exam>& exams) {
  std::vector<int> exam_student_counts;
  exam_student_counts.reserve(exams.size());
  for (const Exam& exam : exams) {
    exam_student_counts.emplace_back(exam.student_count());
  }
  return exam_student_counts;
}

utils::CsrMatrix<int> ProblemData::build_exam_to_sections(const std::vector<Exam>& exams) {
  std::vector<std::vector<int>> exam_to_sections;
  exam_to_sections.reserve(exams.size());
  int index = 0;
  for (const Exam& exam : exams) {
    std::vector<int> v;
    v.reserve(exam.section_count());
    for (int i = 0; i < exam.section_count(); ++i) {
      v.emplace_back(index++);
    }
    exam_to_sections.emplace_back(std::move(v));
  }
  return utils::CsrMatrix<int>(exam_to_sections, index, false);
}

std::vector<int> ProblemData::build_section_to_exam(const utils::CsrMatrix<int>& exam_to_sections) {
  std::vector<int> section_to_exam;
  section_to_exam.reserve(exam_to_sections.num_elements());
  for (int i = 0; i < exam_to_sections.num_rows(); ++i) {
    for (int j = 0; j < exam_to_sections[i].size(); ++j) {
      section_to_exam.emplace_back(i);
    }
  }
  return section_to_exam;
}

std::vector<std::string> ProblemData::extract_room_codes(const std::vector<Room>& rooms) {
  std::vector<std::string> room_codes;
  room_codes.reserve(rooms.size());
  for (const Room& room : rooms) {
    room_codes.emplace_back(room.code());
  }
  return room_codes;
}

std::vector<int> ProblemData::extract_room_capacities(const std::vector<Room>& rooms) {
  std::vector<int> room_capacities;
  room_capacities.reserve(rooms.size());
  for (const Room& room : rooms) {
    room_capacities.emplace_back(room.capacity());
  }
  return room_capacities;
}

std::vector<std::string> ProblemData::extract_room_locations(const std::vector<Room>& rooms) {
  std::vector<std::string> room_locations;
  room_locations.reserve(rooms.size());
  for (const Room& room : rooms) {
    room_locations.emplace_back(room.location());
  }
  return room_locations;
}

std::vector<std::string> ProblemData::extract_room_types(const std::vector<Room>& rooms) {
  std::vector<std::string> room_types;
  room_types.reserve(rooms.size());
  for (const Room& room : rooms) {
    room_types.emplace_back(room.type());
  }
  return room_types;
}

utils::Matrix<int> ProblemData::build_conflicts_matrix(
  const std::vector<Exam>& exams,
  const std::unordered_map<std::string, int>& student_to_id
) {
  std::vector<std::vector<int>> exam_to_students;
  exam_to_students.reserve(exams.size());
  for (const Exam& exam : exams) {
    std::vector<int> students;
    students.reserve(exam.student_count());
    for (const ExamSection& section : exam.sections()) {
      for (const std::string& student : section.students()) {
        int id = student_to_id.at(student);
        students.emplace_back(student_to_id.at(student));
      }
    }
    std::sort(students.begin(), students.end());
    exam_to_students.emplace_back(std::move(students));
  }
  utils::Matrix<int> conflicts_matrix(exams.size(), exams.size(), 0);
  for (int i = 0; i < exams.size(); ++i) {
    const std::vector<int>& students_i = exam_to_students[i];
    for (int j = i + 1; j < exams.size(); ++j) {
      const std::vector<int>& students_j = exam_to_students[j];
      int p = 0;
      int q = 0;
      int common_count = 0;
      while (p < students_i.size() && q < students_j.size()) {
        if (students_i[p] == students_j[q]) {
          ++common_count; ++p; ++q;
        } else if (students_i[p] < students_j[q]) {
          ++p;
        } else {
          ++q;
        }
      }
      conflicts_matrix(i, j) = common_count;
      conflicts_matrix(j, i) = common_count;
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
      PANIC_IF(
        slot >= num_slots,
        "Exam {}: 'feasible_slots' has value {} out of bounds [0, {}]",
        exams[i].code(), slot, num_slots - 1
      );
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
      PANIC_IF(
        room >= num_rooms,
        "Exam {}: 'feasible_rooms' has value {} out of bounds [0, {}]",
        exams[i].code(), room, num_rooms - 1
      );
      feasible_rooms(i, room) = room;
    }
  }
  return utils::CsrMatrix<int>(feasible_rooms, count, false, -1);
}

} // namespace common