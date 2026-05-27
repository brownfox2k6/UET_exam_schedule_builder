#include <numeric>
#include <unordered_set>

#include "common/exams.hpp"

namespace common {

Exam::Exam(
  std::string _code,
  int _credits,
  std::vector<std::string> _students,
  std::vector<int> _feasible_slots,
  std::vector<int> _feasible_rooms,
  std::vector<int> _feasible_proctors
):
  code(std::move(_code)),
  credits(_credits),
  student_count(_students.size()),
  students(std::move(_students)),
  feasible_slots(std::move(_feasible_slots)),
  feasible_rooms(std::move(_feasible_rooms)),
  feasible_proctors(std::move(_feasible_proctors))
{
#ifndef NDEBUG
  utils::panic_if(code.empty(), "Exam code cannot be an empty string");
  utils::panic_if(credits <= 0, "Exam '{}': Must have positive credits (got: {})", code, credits);
  utils::panic_if(students.empty(), "Exam '{}': No registered students", code);
  std::unordered_set<std::string_view> unique_students_checker;
  for (const std::string& s : students) {
    utils::panic_if(s.empty(), "Exam '{}': `students` contains an empty ID", code);
    utils::panic_if(!unique_students_checker.emplace(s).second,
                    "Exam '{}': 'students' contains duplicate ID: '{}'", code, s);
  }
  utils::panic_if(feasible_slots.empty(), "Exam '{}': 'feasible_slots' is empty", code);
  utils::panic_if(feasible_rooms.empty(), "Exam '{}': 'feasible_rooms' is empty", code);
  utils::panic_if(feasible_proctors.empty(), "Exam '{}': 'feasible_proctors' is empty", code);
  std::unordered_set<int> unique_checker;
  for (int slot : feasible_slots) {
    utils::panic_if(slot < 0, "Exam '{}': 'feasible_slots' has a negative value ({})", code, slot);
    utils::panic_if(!unique_checker.emplace(slot).second,
                    "Exam '{}': 'feasible_slots' has duplicate values ({})", code, slot);
  }
  unique_checker.clear();
  for (int room : feasible_rooms) {
    utils::panic_if(room < 0, "Exam '{}': 'feasible_rooms' has a negative value ({})", code, room);
    utils::panic_if(!unique_checker.emplace(room).second,
                    "Exam '{}': 'feasible_rooms' has duplicate values ({})", code, room);
  }
  unique_checker.clear();
  for (int proctor : feasible_proctors) {
    utils::panic_if(proctor < 0, "Exam '{}': 'feasible_proctors' has a negative value ({})", code, proctor);
    utils::panic_if(!unique_checker.emplace(proctor).second,
                    "Exam '{}': 'feasible_proctors' has duplicate values ({})", code, proctor);
  }
#endif // NDEBUG
}

Exams::Exams(
  const std::vector<Exam>& _exams,
  int _num_slots,
  int _num_rooms
):
  num_exams(_exams.size()),
  num_slots(_num_slots),
  num_rooms(_num_rooms),
  codes(extract_codes(_exams)),
  student_counts(extract_student_counts(_exams)),
  credits(extract_credits(_exams)),
  conflicts_matrix(build_conflicts_matrix(_exams)),
  conflicts_csrmatrix(conflicts_matrix),
  total_conflicts(build_total_conflicts(conflicts_csrmatrix)),
  feasible_slots(build_feasible_slots(_exams, _num_slots)),
  feasible_rooms(build_feasible_rooms(_exams, _num_rooms))
{
#ifndef NDEBUG
  utils::panic_if(num_exams == 0, "Exams: '_exams' is empty");
  utils::panic_if(num_slots <= 0, "Exams: '_num_slots' must be positive (got: {})", num_slots);
  utils::panic_if(num_rooms <= 0, "Exams: '_num_rooms' must be positive (got: {})", num_rooms);
  std::unordered_set<std::string_view> unique_codes_checker;
  for (int i = 0; i < num_exams; ++i) {
    utils::panic_if(!unique_codes_checker.emplace(codes[i]).second,
                    "Exams: Duplicate exam code detected: {}", codes[i]);
    for (int slot : _exams[i].feasible_slots) {
      utils::panic_if(slot >= num_slots,
                      "Exam {} has out-of-bounds feasible slot (got: {}, max allowed: {})",
                      codes[i], slot, num_slots - 1);
    }
    for (int room : _exams[i].feasible_rooms) {
      utils::panic_if(room >= num_rooms,
                      "Exam {} has out-of-bounds feasible room (got: {}, max allowed: {})",
                      codes[i], room, num_rooms - 1);
    }
  }
#endif // NDEBUG
}

std::vector<std::string> Exams::extract_codes(const std::vector<Exam>& exams) {
  std::vector<std::string> codes(exams.size());
  for (int i = 0; i < exams.size(); ++i) {
    codes[i] = exams[i].code;
  }
  return codes;
}

std::vector<int> Exams::extract_student_counts(const std::vector<Exam>& exams) {
  std::vector<int> student_counts(exams.size());
  for (int i = 0; i < exams.size(); ++i) {
    student_counts[i] = exams[i].student_count;
  }
  return student_counts;
}

std::vector<int> Exams::extract_credits(const std::vector<Exam>& exams) {
  std::vector<int> credits(exams.size());
  for (int i = 0; i < exams.size(); ++i) {
    credits[i] = exams[i].credits;
  }
  return credits;
}

utils::Matrix<int> Exams::build_conflicts_matrix(const std::vector<Exam>& exams) {
  utils::Matrix<int> conflicts_matrix(exams.size(), exams.size(), 0);
  for (int i = 0; i < exams.size(); ++i) {
    std::unordered_set<std::string_view> students_i;
    students_i.reserve(exams[i].student_count);
    for (const std::string& student_i : exams[i].students) {
      students_i.emplace(student_i);
    }
    for (int j = i + 1; j < exams.size(); ++j) {
      int common_count = 0;
      for (const std::string& student_j : exams[j].students) {
        common_count += students_i.contains(student_j);
      }
      conflicts_matrix(i, j) = conflicts_matrix(j, i) = common_count;
    }
  }
  return conflicts_matrix;
}

std::vector<int> Exams::build_total_conflicts(const utils::CsrMatrix<int>& conflicts_csrmatrix) {
  const int num_exams = conflicts_csrmatrix.num_rows();
  std::vector<int> total_conflicts(num_exams);
  for (int exam = 0; exam < num_exams; ++exam) {
    const auto row = conflicts_csrmatrix[exam].values;
    total_conflicts[exam] = std::accumulate(row.begin(), row.end(), 0);
  }
  return total_conflicts;
}

utils::CsrMatrix<int> Exams::build_feasible_slots(const std::vector<Exam>& exams, int num_slots) {
  utils::Matrix<int> feasible_slots(exams.size(), num_slots, -1);
  int count = 0;
  for (int i = 0; i < exams.size(); ++i) {
    count += exams[i].feasible_slots.size();
    for (int slot : exams[i].feasible_slots) {
      feasible_slots(i, slot) = slot;
    }
  }
  return utils::CsrMatrix<int>(feasible_slots, count, -1, false);
}

utils::CsrMatrix<int> Exams::build_feasible_rooms(const std::vector<Exam>& exams, int num_rooms) {
  utils::Matrix<int> feasible_rooms(exams.size(), num_rooms, -1);
  int count = 0;
  for (int i = 0; i < exams.size(); ++i) {
    count += exams[i].feasible_rooms.size();
    for (int room : exams[i].feasible_rooms) {
      feasible_rooms(i, room) = room;
    }
  }
  return utils::CsrMatrix<int>(feasible_rooms, count, -1, false);
}

} // namespace common