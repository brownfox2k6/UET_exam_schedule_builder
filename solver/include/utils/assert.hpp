#pragma once

#ifndef NDEBUG

#include <format>       // IWYU pragma: keep
#include <iostream>     // IWYU pragma: keep
#include <stdexcept>    // IWYU pragma: keep
#include <string>       // IWYU pragma: keep
#include <string_view>  // IWYU pragma: keep

/**
 * @brief Throws `std::runtime_error` with location data if condition is true (Debug only).
 * @note Evaluates lazily. Compiles to a no-op in Release mode (`NDEBUG`).
 * @param condition Triggers panic if true.
 * @param fmt `std::format` compliant string template.
 * @param ... Arguments for the format string.
 * @throws std::runtime_error Containing the formatted error message.
 */
#define PANIC_IF(condition, fmt, ...)                          \
  do {                                                         \
    if (static_cast<bool>(condition)) {                        \
      std::string error_msg = std::format(                     \
        "\n[Validation Error]\n"                               \
        "  Location: {}:{}\n"                                  \
        "  Function: {}\n"                                     \
        "   Message: {}\n\n",                                  \
        __FILE__, __LINE__, __func__,                          \
        std::format((fmt) __VA_OPT__(,) __VA_ARGS__)           \
      );                                                       \
      throw std::runtime_error(error_msg);                     \
    }                                                          \
  } while (false)

#else

#define PANIC_IF(condition, fmt, ...)                          \
  do {                                                         \
  } while (false)

#endif // NDEBUG