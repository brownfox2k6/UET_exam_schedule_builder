#include "room.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace cetp {

Room::Room(std::string code, int capacity, std::string location, std::string type) {
  set_code(std::move(code));
  set_capacity(capacity);
  set_location(std::move(location));
  set_type(std::move(type));
}

void Room::set_code(std::string code) {
  if (code.empty()) {
    throw std::invalid_argument("Room code cannot be empty");
  }
  code_ = std::move(code);
}

void Room::set_capacity(int capacity) {
  if (capacity <= 0) {
    throw std::invalid_argument("Room capacity must be positive");
  }
  capacity_ = capacity;
}

void Room::set_location(std::string location) {
  if (location.empty()) {
    throw std::invalid_argument("Room location cannot be empty");
  }
  location_ = std::move(location);
}

void Room::set_type(std::string type) {
  if (type.empty()) {
    throw std::invalid_argument("Room type cannot be empty");
  }
  type_ = std::move(type);
}

}  // namespace cetp
