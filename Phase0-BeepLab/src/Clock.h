// One monotonic clock for everything (engines, recorder, schedule), in milliseconds.
// steady_clock is mach_absolute_time on macOS: same source the audio stack uses.
#pragma once

#include <chrono>

namespace beeplab
{
inline double nowMs()
{
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
}
