#pragma once
/*
 * timer.hpp — Portable high-resolution wall-clock timer.
 *
 * Usage:
 *   auto t0 = pdc::now();
 *   // ... work ...
 *   double ms = pdc::elapsedMs(t0);
 */

#include <chrono>

namespace pdc {

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

/* Capture the current wall-clock time. */
inline TimePoint now() {
    return std::chrono::high_resolution_clock::now();
}

/* Return milliseconds elapsed since a previously captured TimePoint. */
inline double elapsedMs(TimePoint start) {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace pdc
