/*
 * This file is part of PokeTimerPlus
 * Port of EonTimer's timers (3.x desktop, MIT, DasAmpharos). A timer is a list of phases in ms;
 * the beeps come at the end of each phase. Checked against EonTimer itself
 * (Test/Resources/eontimer_golden.json, made by Tools/eontimer_golden.py).
 *
 * Calibration values are kept apart from targets so profiles can store them per output device.
 */

#ifndef TIMERS_HPP
#define TIMERS_HPP

#include "Calibrator.hpp"

#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace Timers
{
    inline constexpr double Infinity = std::numeric_limits<double>::infinity(); // open phase (flowing target)

    double toMinimumLength(double value, double minimumLength);
    int minutesBeforeTarget(const std::vector<double> &phases); // infinite phases skipped

    // ---- building blocks
    std::vector<double> secondPhases(const Calibrator &calibrator, int targetSecond, double calibration);
    double secondCalibration(int targetSecond, int secondHit);

    std::vector<double> delayPhases(const Calibrator &calibrator, int targetDelay, int targetSecond, double calibration);
    double delayCalibration(const Calibrator &calibrator, int targetDelay, int delayHit);

    std::vector<double> framePhases(const Calibrator &calibrator, int preTimer, int targetFrame, double calibration);
    double framePhase(const Calibrator &calibrator, int targetFrame, double calibration);
    double frameCalibration(const Calibrator &calibrator, int targetFrame, int frameHit);
    std::vector<double> variableFramePhases(int preTimer);

    std::vector<double> entralinkPhases(const Calibrator &calibrator, int targetDelay, int targetSecond, double calibration,
                                        double entralinkCalibration);
    std::vector<double> enhancedEntralinkPhases(const Calibrator &calibrator, int targetDelay, int targetSecond, int targetAdvances,
                                                double calibration, double entralinkCalibration, double frameCalibration);
    double enhancedEntralinkCalibration(int targetAdvances, int advancesHit);
    inline constexpr double EntralinkAdvancesPerSecond = 0.837148929;
}

// ---- Gen 3

enum class Gen3Mode
{
    Standard,
    VariableTarget
};

struct Gen3Targets
{
    Gen3Mode mode = Gen3Mode::Standard;
    int preTimer = 5000; // ms
    int targetFrame = 1000;
};

namespace Timers
{
    std::vector<double> gen3Phases(const Calibrator &calibrator, const Gen3Targets &targets, double calibration);
    // Amount to add to the Gen 3 calibration (ms) after hitting frameHit
    double gen3CalibrationDelta(const Calibrator &calibrator, const Gen3Targets &targets, int frameHit);
}

// ---- Gen 4

struct Gen4Targets
{
    int targetDelay = 600;
    int targetSecond = 50;
};

struct Gen4Calibration
{
    int calibratedDelay = 500;
    int calibratedSecond = 14;
};

namespace Timers
{
    double gen4Calibration(const Calibrator &calibrator, const Gen4Calibration &calibration); // ms
    std::vector<double> gen4Phases(const Calibrator &calibrator, const Gen4Targets &targets, const Gen4Calibration &calibration);
    // Amount to add to calibratedDelay after hitting delayHit (0 if delayHit <= 0, as EonTimer)
    int gen4CalibratedDelayDelta(const Calibrator &calibrator, const Gen4Targets &targets, int delayHit);
    int gen4MinutesBeforeTarget(const Calibrator &calibrator, const Gen4Targets &targets);
}

// ---- Gen 5

enum class Gen5Mode
{
    Standard,
    CGear,
    Entralink,
    EntralinkPlus
};

struct Gen5Targets
{
    Gen5Mode mode = Gen5Mode::Standard;
    int targetDelay = 1200;
    int targetSecond = 50;
    int targetAdvances = 100;
};

struct Gen5Calibration
{
    int calibration = -95; // delays (ms with precision calibration)
    int entralink = 256; // delays (ms with precision calibration)
    double frame = 0; // ms
};

struct Gen5Hits
{
    std::optional<int> delay;
    std::optional<int> second;
    std::optional<int> advances;
};

namespace Timers
{
    std::vector<double> gen5Phases(const Calibrator &calibrator, const Gen5Targets &targets, const Gen5Calibration &calibration);
    // New calibration after the hits (EonTimer adds to the matching fields depending on the mode)
    Gen5Calibration gen5Calibrate(const Calibrator &calibrator, const Gen5Targets &targets, const Gen5Calibration &calibration,
                                  const Gen5Hits &hits);
    int gen5MinutesBeforeTarget(const Calibrator &calibrator, const Gen5Targets &targets);
}

// ---- Custom

enum class CustomUnit
{
    Milliseconds,
    Advances,
    SeedHex // EonTimer counts it like advances (frames)
};

struct CustomPhase
{
    CustomUnit unit = CustomUnit::Milliseconds;
    int target = 0;
    double calibration = 0; // ms
};

namespace Timers
{
    std::vector<double> customPhases(const Calibrator &calibrator, const std::vector<CustomPhase> &phases);
}

// ---- names, as EonTimer writes them (used in profiles)

std::string_view toString(Gen3Mode mode);
std::string_view toString(Gen5Mode mode);
std::string_view toString(CustomUnit unit);
std::optional<Gen3Mode> gen3ModeFromString(std::string_view s);
std::optional<Gen5Mode> gen5ModeFromString(std::string_view s);
std::optional<CustomUnit> customUnitFromString(std::string_view s);

#endif // TIMERS_HPP
