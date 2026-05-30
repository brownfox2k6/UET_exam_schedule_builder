#pragma once

#include <string>

namespace common {

struct Room {
  std::string code;
  int capacity;
  std::string location;
  std::string type;

  Room(
    std::string _code,
    int _capacity,
    std::string _location = "default",
    std::string _type = "default"
  );
}; // struct Room

} // namespace common