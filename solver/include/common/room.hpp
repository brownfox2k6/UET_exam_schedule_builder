#pragma once

#include <string>

namespace common {

struct Room {
  Room(
    std::string code,
    int capacity,
    std::string location = "default",
    std::string type = "default"
  );
  
  const std::string& code() const { return code_; }
  int capacity() const { return capacity_; }
  const std::string& location() const { return location_; }
  const std::string& type() const { return type_; }

  void set_code(std::string code);
  void set_capacity(int capacity);
  void set_location(std::string location);
  void set_type(std::string type);

private:
  std::string code_;
  int capacity_;
  std::string location_;
  std::string type_;
}; // struct Room

} // namespace common