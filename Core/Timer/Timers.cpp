/*
 * This file is part of PokeTimerPlus
 * Each function mirrors the EonTimer 3.x Python function of the same role (file noted).
 */

#include "Timers.hpp"

#include <cmath>

namespace Timers
{
    // eon_timer/timers/__init__.py
    double toMinimumLength(double value, double minimumLength)
    {
        while (value < minimumLength)
        {
            value += 60000;
        }
        return value;
    }

    int minutesBeforeTarget(const std::vector<double> &phases)
    {
        double total = 0;
        for (double p : phases)
        {
            if (!std::isinf(p))
            {
                total += p;
            }
        }
        return static_cast<int>(std::floor(total / 60000));
    }

    // second_timer.py. EonTimer pads to a fixed 14 s here; the configurable minimum is used instead
    // (as in EonTimer's web rewrite), which is identical at the default.
    std::vector<double> secondPhases(const Calibrator &calibrator, int targetSecond, double calibration)
    {
        return { toMinimumLength(targetSecond * 1000 + calibration + 200, calibrator.minimumLength()) };
    }

    double secondCalibration(int targetSecond, int secondHit)
    {
        if (secondHit < targetSecond)
        {
            return (targetSecond - secondHit) * 1000.0 - 500;
        }
        if (secondHit > targetSecond)
        {
            return (targetSecond - secondHit) * 1000.0 + 500;
        }
        return 0;
    }

    // delay_timer.py
    namespace
    {
        constexpr double CloseThreshold = 167;
        constexpr double UpdateFactor = 1.0;
        constexpr double CloseUpdateFactor = 0.75;
    }

    std::vector<double> delayPhases(const Calibrator &calibrator, int targetDelay, int targetSecond, double calibration)
    {
        double phase1 = toMinimumLength(secondPhases(calibrator, targetSecond, calibration)[0] - calibrator.toMilliseconds(targetDelay),
                                        calibrator.minimumLength());
        double phase2 = calibrator.toMilliseconds(targetDelay) - calibration;
        return { phase1, phase2 };
    }

    double delayCalibration(const Calibrator &calibrator, int targetDelay, int delayHit)
    {
        double delta = calibrator.toMilliseconds(delayHit) - calibrator.toMilliseconds(targetDelay);
        return std::abs(delta) <= CloseThreshold ? CloseUpdateFactor * delta : UpdateFactor * delta;
    }

    // frame_timer.py
    std::vector<double> framePhases(const Calibrator &calibrator, int preTimer, int targetFrame, double calibration)
    {
        return { static_cast<double>(preTimer), framePhase(calibrator, targetFrame, calibration) };
    }

    double framePhase(const Calibrator &calibrator, int targetFrame, double calibration)
    {
        return calibrator.toMilliseconds(targetFrame) + calibration;
    }

    double frameCalibration(const Calibrator &calibrator, int targetFrame, int frameHit)
    {
        return calibrator.toMilliseconds(targetFrame - frameHit);
    }

    std::vector<double> variableFramePhases(int preTimer)
    {
        return { static_cast<double>(preTimer), Infinity };
    }

    // entralink_timer.py
    std::vector<double> entralinkPhases(const Calibrator &calibrator, int targetDelay, int targetSecond, double calibration,
                                        double entralinkCalibration)
    {
        auto phases = delayPhases(calibrator, targetDelay, targetSecond, calibration);
        phases[0] += 250;
        phases[1] -= entralinkCalibration;
        return phases;
    }

    std::vector<double> enhancedEntralinkPhases(const Calibrator &calibrator, int targetDelay, int targetSecond, int targetAdvances,
                                                double calibration, double entralinkCalibration, double frameCalibration)
    {
        auto phases = entralinkPhases(calibrator, targetDelay, targetSecond, calibration, entralinkCalibration);
        phases.push_back((targetAdvances / EntralinkAdvancesPerSecond) * 1000 + frameCalibration);
        return phases;
    }

    double enhancedEntralinkCalibration(int targetAdvances, int advancesHit)
    {
        return ((targetAdvances - advancesHit) / EntralinkAdvancesPerSecond) * 1000;
    }

    // gen3/timer.py
    std::vector<double> gen3Phases(const Calibrator &calibrator, const Gen3Targets &targets, double calibration)
    {
        if (targets.mode == Gen3Mode::VariableTarget)
        {
            return variableFramePhases(targets.preTimer);
        }
        return framePhases(calibrator, targets.preTimer, targets.targetFrame, calibration);
    }

    double gen3CalibrationDelta(const Calibrator &calibrator, const Gen3Targets &targets, int frameHit)
    {
        return frameCalibration(calibrator, targets.targetFrame, frameHit);
    }

    // gen4/timer.py
    double gen4Calibration(const Calibrator &calibrator, const Gen4Calibration &calibration)
    {
        return calibrator.createCalibration(calibration.calibratedDelay, calibration.calibratedSecond);
    }

    std::vector<double> gen4Phases(const Calibrator &calibrator, const Gen4Targets &targets, const Gen4Calibration &calibration)
    {
        return delayPhases(calibrator, targets.targetDelay, targets.targetSecond, gen4Calibration(calibrator, calibration));
    }

    int gen4CalibratedDelayDelta(const Calibrator &calibrator, const Gen4Targets &targets, int delayHit)
    {
        if (delayHit <= 0)
        {
            return 0;
        }
        return calibrator.toDelays(delayCalibration(calibrator, targets.targetDelay, delayHit));
    }

    int gen4MinutesBeforeTarget(const Calibrator &calibrator, const Gen4Targets &targets)
    {
        return minutesBeforeTarget(delayPhases(calibrator, targets.targetDelay, targets.targetSecond, 0));
    }

    // gen5/timer.py
    std::vector<double> gen5Phases(const Calibrator &calibrator, const Gen5Targets &targets, const Gen5Calibration &calibration)
    {
        double c = calibrator.calibrateToMilliseconds(calibration.calibration);
        double ec = calibrator.calibrateToMilliseconds(calibration.entralink);
        switch (targets.mode)
        {
        case Gen5Mode::Standard:
            return secondPhases(calibrator, targets.targetSecond, c);
        case Gen5Mode::CGear:
            return delayPhases(calibrator, targets.targetDelay, targets.targetSecond, c);
        case Gen5Mode::Entralink:
            return entralinkPhases(calibrator, targets.targetDelay, targets.targetSecond, c, ec);
        case Gen5Mode::EntralinkPlus:
            return enhancedEntralinkPhases(calibrator, targets.targetDelay, targets.targetSecond, targets.targetAdvances, c, ec,
                                           calibration.frame);
        }
        return {};
    }

    Gen5Calibration gen5Calibrate(const Calibrator &calibrator, const Gen5Targets &t, const Gen5Calibration &calibration,
                                  const Gen5Hits &hits)
    {
        Gen5Calibration out = calibration;
        switch (t.mode)
        {
        case Gen5Mode::Standard:
            if (hits.second)
            {
                out.calibration += calibrator.calibrateToDelays(secondCalibration(t.targetSecond, *hits.second));
            }
            break;
        case Gen5Mode::CGear:
            if (hits.delay)
            {
                out.calibration += calibrator.calibrateToDelays(delayCalibration(calibrator, t.targetDelay, *hits.delay));
            }
            break;
        case Gen5Mode::Entralink:
        case Gen5Mode::EntralinkPlus:
            if (hits.second && *hits.second != t.targetSecond)
            {
                out.calibration += calibrator.calibrateToDelays(secondCalibration(t.targetSecond, *hits.second));
            }
            if (hits.delay && *hits.delay != t.targetDelay)
            {
                out.entralink += calibrator.calibrateToDelays(delayCalibration(calibrator, t.targetDelay, *hits.delay));
            }
            if (t.mode == Gen5Mode::EntralinkPlus && hits.advances && *hits.advances != t.targetAdvances)
            {
                out.frame += enhancedEntralinkCalibration(t.targetAdvances, *hits.advances);
            }
            break;
        }
        return out;
    }

    int gen5MinutesBeforeTarget(const Calibrator &calibrator, const Gen5Targets &t)
    {
        switch (t.mode)
        {
        case Gen5Mode::Standard:
            return minutesBeforeTarget(secondPhases(calibrator, t.targetSecond, 0));
        case Gen5Mode::CGear:
            return minutesBeforeTarget(delayPhases(calibrator, t.targetDelay, t.targetSecond, 0));
        case Gen5Mode::Entralink:
        case Gen5Mode::EntralinkPlus:
            return minutesBeforeTarget(entralinkPhases(calibrator, t.targetDelay, t.targetSecond, 0, 0));
        }
        return 0;
    }

    // custom/timer.py
    std::vector<double> customPhases(const Calibrator &calibrator, const std::vector<CustomPhase> &phases)
    {
        std::vector<double> out;
        out.reserve(phases.size());
        for (const auto &p : phases)
        {
            double value = p.unit == CustomUnit::Milliseconds ? static_cast<double>(p.target) : calibrator.toMilliseconds(p.target);
            out.push_back(value + p.calibration);
        }
        return out;
    }
}

std::string_view toString(Gen3Mode mode)
{
    return mode == Gen3Mode::VariableTarget ? "Variable Target" : "Standard";
}

std::string_view toString(Gen5Mode mode)
{
    switch (mode)
    {
    case Gen5Mode::CGear:
        return "C-Gear";
    case Gen5Mode::Entralink:
        return "Entralink";
    case Gen5Mode::EntralinkPlus:
        return "Entralink+";
    default:
        return "Standard";
    }
}

std::string_view toString(CustomUnit unit)
{
    switch (unit)
    {
    case CustomUnit::Advances:
        return "Advances";
    case CustomUnit::SeedHex:
        return "Seed (Hex)";
    default:
        return "ms";
    }
}

std::optional<Gen3Mode> gen3ModeFromString(std::string_view s)
{
    for (Gen3Mode m : { Gen3Mode::Standard, Gen3Mode::VariableTarget })
    {
        if (toString(m) == s)
        {
            return m;
        }
    }
    return std::nullopt;
}

std::optional<Gen5Mode> gen5ModeFromString(std::string_view s)
{
    for (Gen5Mode m : { Gen5Mode::Standard, Gen5Mode::CGear, Gen5Mode::Entralink, Gen5Mode::EntralinkPlus })
    {
        if (toString(m) == s)
        {
            return m;
        }
    }
    return std::nullopt;
}

std::optional<CustomUnit> customUnitFromString(std::string_view s)
{
    for (CustomUnit u : { CustomUnit::Milliseconds, CustomUnit::Advances, CustomUnit::SeedHex })
    {
        if (toString(u) == s)
        {
            return u;
        }
    }
    return std::nullopt;
}
