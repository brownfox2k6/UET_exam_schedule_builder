#include "problem_data.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "exam.hpp"
#include "room.hpp"
#include "utils/csr_matrix.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace {

using namespace cetp;

auto validate_input(const Vector<Exam>& exams, const Vector<Room>& rooms)
    -> std::unordered_map<std::string_view, int> {
  if (exams.empty()) {
    throw std::invalid_argument("ProblemData: exams must not be empty");
  }
  if (rooms.empty()) {
    throw std::invalid_argument("ProblemData: rooms must not be empty");
  }

  const int num_rooms = rooms.size();

  std::unordered_map<std::string_view, int> room_ids;
  room_ids.reserve(size_t(num_rooms));
  for (int room_id = 0; room_id < num_rooms; ++room_id) {
    const std::string_view code = rooms[room_id].code();
    if (!room_ids.emplace(code, room_id).second) {
      throw std::invalid_argument(std::format("ProblemData: duplicate room code '{}'", code));
    }
  }

  std::unordered_set<std::string_view> unique_exams;
  unique_exams.reserve(size_t(exams.size()));
  for (const Exam& exam : exams) {
    if (!unique_exams.emplace(exam.code).second) {
      throw std::invalid_argument(std::format("ProblemData: duplicate exam code '{}'", exam.code));
    }
    for (const ExamSection& section : exam.sections) {
      for (std::string_view room_code : section.feasible_rooms) {
        if (!room_ids.contains(room_code)) {
          throw std::invalid_argument(
              std::format("Section '{}': unknown feasible room code '{}'", section.code, room_code)
          );
        }
      }
    }
  }

  return room_ids;
}

void populate_exams(ProblemData& data, const Vector<Exam>& exams) {
  assert(!exams.empty());

  data.num_exams = exams.size();

  data.exam_codes.reserve(data.num_exams);
  data.exam_credits.reserve(data.num_exams);
  data.exam_student_counts.reserve(data.num_exams);

  for (const Exam& exam : exams) {
    data.num_sections += exam.sections.size();
    data.num_enrollments += exam.student_count;

    data.exam_codes.emplace_back(exam.code);
    data.exam_credits.push_back(exam.credits);
    data.exam_student_counts.push_back(exam.student_count);
  }
}

void populate_students(ProblemData& data, const Vector<Exam>& exams) {
  // Push all enrollments inside temp
  Vector<std::string_view> temp;
  temp.reserve(data.num_enrollments);
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections) {
      for (std::string_view student : section.students) {
        temp.push_back(student);
      }
    }
  }
  // Deduplicate enrollments -> we have unique student IDs
  std::ranges::sort(temp);
  const auto unique_end = std::ranges::unique(temp).begin();
  data.num_students = int(unique_end - temp.begin());
  data.student_codes.reserve(data.num_students);
  for (auto it = temp.begin(); it != unique_end; ++it) {
    data.student_codes.emplace_back(*it);
  }
}

void populate_sections(ProblemData& data, const Vector<Exam>& exams) {
  data.exam_section_offsets.reserve(data.num_exams + 1);
  data.section_to_exam.reserve(data.num_sections);
  data.section_codes.reserve(data.num_sections);
  data.section_student_counts.reserve(data.num_sections);

  data.section_students.reserve_num_rows(data.num_sections);
  data.section_students.reserve_num_elements(data.num_enrollments);

  int exam_index = 0;
  for (const Exam& exam : exams) {
    data.exam_section_offsets.push_back(data.section_to_exam.size());
    for (const ExamSection& section : exam.sections) {
      data.section_to_exam.push_back(exam_index);
      data.section_codes.emplace_back(section.code);
      data.section_student_counts.emplace_back(section.students.size());
      for (std::string_view student : section.students) {
        data.section_students.append(data.find_student_id(student));
      }
      data.section_students.finish_row();
    }
    ++exam_index;
  }
  data.exam_section_offsets.push_back(data.num_sections);
}

auto build_exams_by_student(const ProblemData& data) -> CsrMatrix<int> {
  Vector<int> offsets(data.num_students + 1, 0);
  for (int section : data.sections()) {
    for (int student : data.section_students[section].values) {
      ++offsets[student + 1];
    }
  }
  for (int student : data.students()) {
    offsets[student + 1] += offsets[student];
  }
  Vector<int> values(data.num_enrollments);
  Vector<int> cursor = offsets;
  for (int exam : data.exams()) {
    for (int section : data.sections_of_exam(exam)) {
      for (int student : data.section_students[section].values) {
        values[cursor[student]++] = exam;
      }
    }
  }
  return {std::move(values), std::move(offsets)};
}

void populate_conflicts(ProblemData& data) {
  const CsrMatrix<int> exams_by_student = build_exams_by_student(data);

  data.conflicts_matrix = Matrix<int>(data.num_exams, data.num_exams, 0);
  for (int student : data.students()) {
    const auto exams = exams_by_student[student].values;
    for (int i = 0; i < exams.size(); ++i) {
      for (int j = i + 1; j < exams.size(); ++j) {
        ++data.conflicts_matrix[exams[i], exams[j]];
        ++data.conflicts_matrix[exams[j], exams[i]];
      }
    }
  }

  data.conflicts_csr = CsrMatrix<int>(data.conflicts_matrix);

  data.weighted_conflict_degrees.reserve(data.num_exams);
  for (int exam : data.exams()) {
    const auto row = data.conflicts_csr[exam].values;
    const int count = std::accumulate(row.begin(), row.end(), 0);
    data.weighted_conflict_degrees.push_back(count);
  }
}

void populate_rooms(ProblemData& data, const Vector<Room>& rooms) {
  assert(!rooms.empty());

  data.num_rooms = rooms.size();

  data.room_codes.reserve(data.num_rooms);
  data.room_capacities.reserve(data.num_rooms);
  data.room_locations.reserve(data.num_rooms);
  data.room_types.reserve(data.num_rooms);

  for (const Room& room : rooms) {
    data.room_codes.emplace_back(room.code());
    data.room_capacities.push_back(room.capacity());
    data.room_locations.emplace_back(room.location());
    data.room_types.emplace_back(room.type());
  }
}

void populate_timestamps(ProblemData& data, const Vector<Exam>& exams) {
  int count = 0;
  for (const Exam& exam : exams) {
    count += exam.feasible_timestamps.size();
  }
  Vector<int64_t> timestamps;
  timestamps.reserve(count);
  for (const Exam& exam : exams) {
    for (int64_t timestamp : exam.feasible_timestamps) {
      timestamps.push_back(timestamp);
    }
  }
  std::ranges::sort(timestamps);
  const auto unique_end = std::ranges::unique(timestamps).begin();
  timestamps.erase(unique_end, timestamps.end());

  data.num_slots = timestamps.size();
  data.slot_timestamps = std::move(timestamps);
}

void populate_feasible_slots(ProblemData& data, const Vector<Exam>& exams) {
  int total_num_feasible_slots = 0;
  for (const Exam& exam : exams) {
    total_num_feasible_slots += exam.feasible_timestamps.size();
  }
  data.feasible_slots_by_exam.reserve_num_rows(data.num_exams);
  data.feasible_slots_by_exam.reserve_num_elements(total_num_feasible_slots);
  for (const Exam& exam : exams) {
    for (int64_t timestamp : exam.feasible_timestamps) {
      const auto iter = std::ranges::lower_bound(data.slot_timestamps, timestamp);
      assert(iter != data.slot_timestamps.end() && *iter == timestamp);
      const int timeslot = int(iter - data.slot_timestamps.begin());
      data.feasible_slots_by_exam.append(timeslot);
    }
    data.feasible_slots_by_exam.finish_row();
  }
}

void populate_feasible_rooms(
    ProblemData& data,
    const Vector<Exam>& exams,
    const std::unordered_map<std::string_view, int>& room_ids
) {
  int total_num_feasible_rooms = 0;
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections) {
      total_num_feasible_rooms += section.feasible_rooms.size();
    }
  }
  data.feasible_rooms_by_section.reserve_num_rows(data.num_sections);
  data.feasible_rooms_by_section.reserve_num_elements(total_num_feasible_rooms);

  Vector<int> section_rooms;
  for (const Exam& exam : exams) {
    for (const ExamSection& section : exam.sections) {
      section_rooms.clear();
      section_rooms.reserve(section.feasible_rooms.size());
      for (std::string_view room_code : section.feasible_rooms) {
        const auto iter = room_ids.find(room_code);
        assert(iter != room_ids.end());
        section_rooms.push_back(iter->second);
      }
      std::ranges::sort(section_rooms);
      data.feasible_rooms_by_section.append_row(section_rooms);
    }
  }
}

}  // namespace

namespace cetp {

ProblemData::ProblemData(const Vector<Exam>& exams, const Vector<Room>& rooms) {
  const auto room_ids = validate_input(exams, rooms);
  populate_exams(*this, exams);
  populate_students(*this, exams);
  populate_sections(*this, exams);
  populate_conflicts(*this);
  populate_rooms(*this, rooms);
  populate_timestamps(*this, exams);
  populate_feasible_slots(*this, exams);
  populate_feasible_rooms(*this, exams, room_ids);
}

auto ProblemData::sections_of_exam(int exam) const noexcept -> std::ranges::iota_view<int, int> {
  assert(exam >= 0 && exam < num_exams);
  const int begin = exam_section_offsets[exam];
  const int end = exam_section_offsets[exam + 1];
  return std::views::iota(begin, end);
}

auto ProblemData::find_student_id(std::string_view student) const noexcept -> int {
  const auto iter = std::ranges::lower_bound(student_codes, student);
  if (iter != student_codes.end() && *iter == student) {
    return int(iter - student_codes.begin());
  }
  return -1;
}

}  // namespace cetp
