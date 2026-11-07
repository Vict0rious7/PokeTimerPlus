/*
 * This file is part of PokeTimerPlus
 */

#include "Schedule.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

Schedule::Schedule(std::vector<double> phases, int beepCount, double beepIntervalMs) :
    phases_(std::move(phases)), beepCount_(beepCount), interval_(beepIntervalMs)
{
}

const std::vector<double> &Schedule::phases() const
{
    return phases_;
}

double Schedule::phaseStart(int phase) const
{
    double t = 0;
    for (int i = 0; i < phase && i < static_cast<int>(phases_.size()); i++)
    {
        t += phases_[i];
    }
    return t;
}

double Schedule::phaseEnd(int phase) const
{
    return phaseStart(phase + 1);
}

double Schedule::totalMs() const
{
    return phaseStart(static_cast<int>(phases_.size()));
}

std::vector<Beep> Schedule::beeps() const
{
    // eon_timer/phase_runner.py __build_actions: at the start of a phase, the beeps are the phase end
    // minus k intervals, for k < count and k * interval < phase length.
    std::vector<Beep> out;
    double start = 0;
    for (int p = 0; p < static_cast<int>(phases_.size()); p++)
    {
        double length = phases_[p];
        if (std::isinf(length))
        {
            break;
        }
        double end = start + length;
        for (int k = beepCount_ - 1; k >= 0; k--)
        {
            double before = k * interval_;
            if (before < length)
            {
                out.push_back({ end - before, p, k });
            }
        }
        start = end;
    }
    return out;
}

std::vector<Beep> Schedule::beepsAfter(double ms) const
{
    std::vector<Beep> out;
    for (const Beep &b : beeps())
    {
        if (b.ms > ms)
        {
            out.push_back(b);
        }
    }
    return out;
}

bool Schedule::setPhase(int phase, double durationMs, double nowMs)
{
    if (phase < 0 || phase >= static_cast<int>(phases_.size()))
    {
        return false;
    }
    phases_[phase] = durationMs;
    return phaseEnd(phase) > nowMs;
}

void Schedule::appendPhase(double durationMs)
{
    phases_.push_back(durationMs);
}

int Schedule::phaseAt(double nowMs) const
{
    double end = 0;
    for (int p = 0; p < static_cast<int>(phases_.size()); p++)
    {
        end += phases_[p];
        if (nowMs < end)
        {
            return p;
        }
    }
    return static_cast<int>(phases_.size());
}

std::vector<Beep> beepsForTargets(const std::vector<double> &targetsMs, int beepCount, double beepIntervalMs)
{
    std::vector<Beep> out;
    for (int t = 0; t < static_cast<int>(targetsMs.size()); t++)
    {
        for (int k = beepCount - 1; k >= 0; k--)
        {
            double ms = targetsMs[t] - k * beepIntervalMs;
            if (ms >= 0)
            {
                out.push_back({ ms, t, k });
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const Beep &a, const Beep &b) { return a.ms < b.ms; });
    return out;
}
