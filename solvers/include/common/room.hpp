#pragma once

#include <string>

namespace common {

struct Room {
  Room(std::string code, int capacity, std::string location = "default",
       std::string type = "default");

  [[nodiscard]] auto code() const -> const std::string& { return code_; }
  [[nodiscard]] auto capacity() const -> int { return capacity_; }
  [[nodiscard]] auto location() const -> const std::string& { return location_; }
  [[nodiscard]] auto type() const -> const std::string& { return type_; }

  void set_code(std::string code);
  void set_capacity(int capacity);
  void set_location(std::string location);
  void set_type(std::string type);

 private:
  std::string code_;
  int capacity_ = 0;
  std::string location_;
  std::string type_;
};  // struct Room

}  // namespace common