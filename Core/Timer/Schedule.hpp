/*
 * This file is part of PokeTimerPlus
 * Turns a timer's phases into beep times, and lets phases change while the timer runs
 * (FlowTimer-style "flowing" targets, EonTimer's Variable Target).
 */

#ifndef SCHEDULE_HPP
#define SCHEDULE_HPP

#include <vector>

struct Beep
{
    double ms; // from the timer start
    int phase; // phase it ends
    int fromEnd; // 0 = the beep on the target itself, 1 = one interval before, ...
};

class Schedule
{
public:
    /**
     * @param phases Phase lengths in ms (Timers::Infinity = open, its target not known yet)
     * @param beepCount Beeps per phase, the last one on the phase end (EonTimer "count")
     * @param beepIntervalMs Spacing between beeps (EonTimer "interval")
     */
    Schedule(std::vector<double> phases, int beepCount, double beepIntervalMs);

    const std::vector<double> &phases() const;
    double phaseStart(int phase) const; // Infinity if an earlier phase is open
    double phaseEnd(int phase) const;
    double totalMs() const; // Infinity while any phase is open

    /**
     * @brief Every beep that can be placed now, in time order. Stops at the first open phase.
     *
     * Like EonTimer, a phase shorter than (count - 1) intervals gets only the beeps that fit.
     */
    std::vector<Beep> beeps() const;

    /**
     * @brief Beeps after a given time (what an audio engine still has to play)
     */
    std::vector<Beep> beepsAfter(double ms) const;

    /**
     * @brief Changes a phase while running (flowing target)
     *
     * @param phase Phase index
     * @param durationMs New length (may be Infinity to reopen it)
     * @param nowMs Elapsed time since the timer start
     *
     * @return false if the new end of that phase is already in the past (EonTimer then ends the
     *         phase at once); the change is applied either way
     */
    bool setPhase(int phase, double durationMs, double nowMs);

    /**
     * @brief Adds a phase after the last one (another target in the same run)
     */
    void appendPhase(double durationMs);

    /**
     * @brief Index of the phase running at nowMs, or phase count when finished
     */
    int phaseAt(double nowMs) const;

private:
    std::vector<double> phases_;
    int beepCount_;
    double interval_;
};

/**
 * @brief Continuous timer (FlowTimer-style, for Gen 6+): the clock runs until stopped and targets are
 *        added while it runs. Beeps for each target end on it, beepCount of them, interval apart;
 *        beeps that would fall before the start are left out.
 *
 * @param targetsMs Targets in ms from the timer start (any order)
 */
std::vector<Beep> beepsForTargets(const std::vector<double> &targetsMs, int beepCount, double beepIntervalMs);

#endif // SCHEDULE_HPP
