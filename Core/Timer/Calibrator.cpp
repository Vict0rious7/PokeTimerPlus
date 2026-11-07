/*
 * This file is part of PokeTimerPlus
 */

#include "Calibrator.hpp"

#include <cfenv>
#include <cmath>

double roundHalfEven(double value)
{
    // nearbyint uses the current rounding mode; the default (and the one we force) is
    // to-nearest-even, which is exactly Python's round() on the binary value.
    const int mode = std::fegetround();
    if (mode != FE_TONEAREST)
    {
        std::fesetround(FE_TONEAREST);
    }
    double r = std::nearbyint(value);
    if (mode != FE_TONEAREST)
    {
        std::fesetround(mode);
    }
    return r;
}

Calibrator::Calibrator(double fps, bool precisionCalibration, int minimumLengthSeconds) :
    msPerFrame_(1000.0 / fps), precision_(precisionCalibration), minimumLengthSeconds_(minimumLengthSeconds)
{
}

double Calibrator::msPerFrame() const
{
    return msPerFrame_;
}

bool Calibrator::precisionCalibration() const
{
    return precision_;
}

double Calibrator::minimumLength() const
{
    return minimumLengthSeconds_ * 1000.0;
}

int Calibrator::toDelays(double milliseconds) const
{
    return static_cast<int>(roundHalfEven(milliseconds / msPerFrame_));
}

double Calibrator::toMilliseconds(int delays) const
{
    return roundHalfEven(msPerFrame_ * delays);
}

int Calibrator::calibrateToDelays(double milliseconds) const
{
    return precision_ ? static_cast<int>(roundHalfEven(milliseconds)) : toDelays(milliseconds);
}

double Calibrator::calibrateToMilliseconds(int delays) const
{
    return precision_ ? static_cast<double>(delays) : toMilliseconds(delays);
}

double Calibrator::createCalibration(int delays, double seconds) const
{
    return toMilliseconds(delays - toDelays(seconds * 1000));
}
