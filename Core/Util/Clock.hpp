/*
 * This file is part of PokeTimerPlus
 * One monotonic clock for the whole app, in milliseconds (steady_clock = mach_absolute_time on macOS).
 */

#ifndef CLOCK_HPP
#define CLOCK_HPP

#include <chrono>

namespace Clock
{
    inline double nowMs()
    {
        using namespace std::chrono;
        return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
    }
}

#endif // CLOCK_HPP
