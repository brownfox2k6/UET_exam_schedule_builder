#pragma once

#include <cstdlib>
#include <format>
#include <iostream>
#include <ostream>
#include <string_view>

namespace utils {

#ifndef NDEBUG

/**
 * @brief Asserts a condition during Debug mode and aborts if true.
 * @note Formatting is lazily evaluated only when the condition is met.
 * @tparam Args Types for the formatting arguments.
 * @param condition Triggers panic if true.
 * @param fmt `std::format` compliant string template.
 * @param args Perfect-forwarded arguments for the format string.
 */
template <typename... Args>
inline void panic_if(bool condition, std::string_view fmt, Args&&... args) {
  if (condition) {
    std::cerr << "[Validation Error] " 
              << std::vformat(fmt, std::make_format_args(args...)) 
              << ".\n"
              << std::flush;
    std::abort();
  }
}

#else

/**
 * @brief Zero-cost no-op stub for `panic_if` during Release mode.
 */
template <typename... Args>
inline void panic_if(bool, std::string_view, Args&&...) {}

#endif // NDEBUG

} // namespace utils