#pragma once
#include <Arduino.h>

// Compatibility helpers for the ESP32 Arduino core/toolchain used by this project.
// Range-based for over Arduino String is not available in every core revision.
inline const char *begin(const String &s) { return s.c_str(); }
inline const char *end(const String &s) { return s.c_str() + s.length(); }

// main.cpp uses an explicit typed max<uint32_t>() once. Parenthesizing the
// declaration avoids Arduino's two-argument max macro while still providing
// the typed overload expected by that call.
template <typename T>
constexpr T (max)(T a, T b) { return a > b ? a : b; }
