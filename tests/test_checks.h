#pragma once

#include <cmath>
#include <stdexcept>
#include <string>

// Throw on failed checks in Debug and Release. CTest reports the exception message.
inline void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

inline void requireNear(float actual, float expected, const std::string& message, float tolerance = 1e-4f)
{
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance,
            message + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}
