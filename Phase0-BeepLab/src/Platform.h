// macOS-specific helpers (stubs elsewhere): App Nap, Low Power Mode, power source.
#pragma once

#include <QString>

namespace platform
{
// While true, macOS is asked not to throttle the app (no App Nap, latency-critical timers).
void setKeepAwake(bool on);
bool lowPowerMode();
QString powerSource(); // "AC", "Battery" or "?"
}
