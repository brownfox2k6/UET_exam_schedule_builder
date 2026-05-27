#pragma once

#ifndef NDEBUG

#include <format>       // IWYU pragma: keep
#include <iostream>     // IWYU pragma: keep
#include <stdexcept>    // IWYU pragma: keep
#include <string>       // IWYU pragma: keep
#include <string_view>  // IWYU pragma: keep

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