#pragma once

#include <cstdint>

#include "utils/csr_matrix.hpp"
#include "utils/span.hpp"

namespace cetp::rooms {

enum class RoomAssignmentStatus : std::uint8_t {
  OPTIMAL,
  FEASIBLE,
  INFEASIBLE,
  UNKNOWN,
  MODEL_INVALID
};

/** Room allocation for a fixed schedule. Room IDs are stored as CSR values, including ID zero. */
struct RoomAssignment {
  // NOLINTBEGIN(*-non-private-*)
  RoomAssignmentStatus status{};
  CsrMatrix<int> rooms_by_section;
  int64_t unused_capacity{};
  // NOLINTEND(*-non-private-*)

  [[nodiscard]] auto rooms_for_section(int section) const -> Span<const int>;
};  // struct RoomAssignment

}  // namespace cetp::rooms
