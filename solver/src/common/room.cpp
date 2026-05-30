#include "common/room.hpp"
#include "utils/assert.hpp"

namespace common {

Room::Room(
  std::string code,
  int capacity,
  std::string location,
  std::string type
) {
  set_code(std::move(code));
  set_capacity(capacity);
  set_location(std::move(location));
  set_type(std::move(type));
}

void Room::set_code(std::string code) {
  PANIC_IF(code.empty(), "Room code cannot be empty");
  code_ = std::move(code);
}

void Room::set_capacity(int capacity) {
  PANIC_IF(capacity <= 0, "Room '{}': capacity must be positive (got: {})", code_, capacity);
  capacity_ = capacity;
}

void Room::set_location(std::string location) {
  PANIC_IF(location.empty(), "Room '{}': location cannot be empty", code_);
  location_ = std::move(location);
}

void Room::set_type(std::string type) {
  PANIC_IF(type.empty(), "Room '{}': type cannot be empty", code_);
  type_ = std::move(type);
}

} // namespace common