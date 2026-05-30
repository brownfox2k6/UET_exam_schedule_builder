#include "common/room.hpp"
#include "utils/assert.hpp"

namespace common {

Room::Room(
  std::string _code,
  int _capacity,
  std::string _location,
  std::string _type
):
  code(std::move(_code)),
  capacity(_capacity),
  location(std::move(_location)),
  type(std::move(_type))
{
#ifndef NDEBUG
  PANIC_IF(code.empty(), "Room code cannot be empty");
  PANIC_IF(capacity <= 0, "Room '{}': capacity must be positive (got: {})", code, capacity);
  PANIC_IF(location.empty(), "Room '{}': location cannot be empty", code);
  PANIC_IF(type.empty(), "Room '{}': type cannot be empty", code);
#endif // NDEBUG
}

} // namespace common