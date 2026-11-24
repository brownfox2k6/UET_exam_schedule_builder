#pragma once

#include <cassert>
#include <cstdint>
#include <random>

/** Creates a deterministic MT19937 engine seeded from a 64-bit base seed. */
[[nodiscard]] inline auto make_random_engine(uint64_t base_seed) -> std::mt19937 {
  std::seed_seq seed_sequence{
      uint32_t(base_seed),
      uint32_t(base_seed >> 32)  // NOLINT(*-magic-numbers)
  };
  return std::mt19937(seed_sequence);
}

/** Creates an independent MT19937 stream for parallel workers sharing a base seed. */
[[nodiscard]] inline auto make_random_engine(uint64_t base_seed, int stream) -> std::mt19937 {
  std::seed_seq seed_sequence{
      uint32_t(base_seed),
      uint32_t(base_seed >> 32),  // NOLINT(*-magic-numbers)
      uint32_t(stream),
      0x9e3779b9U  // NOLINT(*-magic-numbers) Golden-ratio seed mixer
  };
  return std::mt19937(seed_sequence);
}

/** Returns a uniform random integer in the half-open interval [min_value, max_value). */
[[nodiscard]] inline auto random_int(std::mt19937& engine, int min_value, int max_value) -> int {
  assert(min_value < max_value);
  return std::uniform_int_distribution<int>(min_value, max_value - 1)(engine);
}

/** Returns a uniform random double in the half-open interval [min_value, max_value). */
[[nodiscard]] inline auto random_double(std::mt19937& engine, double min_value, double max_value)
    -> double {
  assert(min_value <= max_value);
  return std::uniform_real_distribution<double>(min_value, max_value)(engine);
}
