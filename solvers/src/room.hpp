#pragma once

#include <string>

namespace cetp {

class Room {
public:
  Room(std::string code, int capacity, std::string location, std::string type);

  [[nodiscard]] auto code() const noexcept -> const std::string& { return code_; }
  [[nodiscard]] auto capacity() const noexcept -> int { return capacity_; }
  [[nodiscard]] auto location() const noexcept -> const std::string& { return location_; }
  [[nodiscard]] auto type() const noexcept -> const std::string& { return type_; }

  void set_code(std::string code);
  void set_capacity(int capacity);
  void set_location(std::string location);
  void set_type(std::string type);

private:
  std::string code_;
  int capacity_ = 0;
  std::string location_;
  std::string type_;
};  // class Room

}  // namespace cetp
