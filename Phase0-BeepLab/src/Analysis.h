// Beep timing analysis. Pure C++ (no Qt) so it can be unit-tested on synthetic recordings.
//
// A run is: the app schedules beeps at known system times, the microphone records what actually
// came out of the speaker, and we find each beep in the recording. The constant part of the error
// (output + input latency, distance to the mic) is reported as "latency" and would be absorbed by
// calibration. What matters for RNG is everything that is NOT constant: jitter, a late first beep,
// drift, uneven intervals.
#pragma once

#include <cstdint>
#include <limits>
#include <vector>

namespace beeplab
{
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// One capture chunk: total frames received so far, and the system time (ms) when it arrived.
struct Stamp
{
    int64_t frames;
    double ms;
};

struct Recording
{
    int sampleRate = 0;
    std::vector<float> samples; // mono
    std::vector<Stamp> stamps;
};

// Maps capture frames to system milliseconds. The microphone's sample clock is trusted for the
// rate (nominal sample rate: a crystal, steadier than chunk delivery times); the stamps only give
// the offset (median), so delivery delays only add a constant, which ends up in "latency".
// Any rate error of the mic clock shows up as a straight line, i.e. in "drift", not in jitter.
struct ClockMap
{
    double a = 0; // ms at frame 0
    double b = 0; // ms per frame
    double toMs(double frame) const { return a + b * frame; }
    double toFrame(double ms) const { return (ms - a) / b; }
};
ClockMap fitClock(const std::vector<Stamp> &stamps, int sampleRate);
// Mic clock rate vs system clock from the stamps (least squares), in ppm. Diagnostic only.
double micClockPpm(const std::vector<Stamp> &stamps, int sampleRate);

// Finds a known sound in a recording. Both are band-passed around the sound's main frequency,
// turned into an envelope, and compared by correlation of envelopes. Envelopes avoid locking onto
// the wrong cycle of a pure tone; the band-pass keeps music or speech from matching.
class BeepDetector
{
public:
    struct Hit
    {
        bool found = false;
        double frame = 0; // recording frame where the sound starts
        double score = 0; // envelope correlation, 0..1
        double snr = 0;   // envelope peak / noise floor
    };

    BeepDetector(const std::vector<float> &sound, int soundRate, int recordingRate);
    void setRecording(const std::vector<float> &samples);

    Hit best(double fromFrame, double toFrame) const;     // strongest match in the range
    Hit earliest(double fromFrame, double toFrame) const; // first acceptable match in the range
    std::vector<Hit> all(double minGapFrames) const;      // every match, at least minGapFrames apart

    double toneHz() const { return toneHz_; }

    double minScore = 0.6;
    double minSnr = 4.0;

private:
    std::vector<float> process(const std::vector<float> &x) const;
    double scoreAt(int64_t k) const;
    double snrAt(int64_t k) const;
    bool accept(int64_t k) const;
    Hit refine(int64_t k) const;
    int64_t toEnv(double frame) const;

    int rate_;
    int decim_;
    double toneHz_;
    std::vector<float> tmpl_; // template envelope, mean removed
    double tmplNorm_ = 0;
    std::vector<float> env_;
    std::vector<double> sum_, sum2_;
    double floor_ = 1e-9;
};

struct ScheduledBeep
{
    double ms = 0;      // when it should be heard (system clock)
    int group = 0;      // countdown group
    int indexInGroup = 0;
    double triggerMs = kNaN; // when the engine actually fired it (only engines that fire at play time)
};

struct BeepResult
{
    ScheduledBeep scheduled;
    bool detected = false;
    double measuredMs = kNaN;
    double offsetMs = kNaN;   // measured - scheduled
    double residualMs = kNaN; // offset minus the fitted latency + drift line
    double score = 0;
    double snr = 0;
};

struct RunMetrics
{
    int total = 0;
    int detected = 0;
    double latencyMs = kNaN;        // median offset (constant part, calibrated away)
    double jitterMs = kNaN;         // SD of residuals
    double maxAbsMs = kNaN;         // worst residual
    double firstBeepDeltaMs = kNaN; // mean residual of the first beep of each group minus the others
    double driftPpm = kNaN;         // slope of offset over time (output clock vs microphone clock)
    double micClockPpm = kNaN;      // microphone clock vs system clock, from chunk arrival times
    double intervalSdMs = kNaN;     // SD of (measured - scheduled) beep-to-beep interval inside groups
    double intervalMaxAbsMs = kNaN;
    double triggerLateMeanMs = kNaN; // engine fired late by (only for fire-at-play-time engines)
    double triggerLateMaxMs = kNaN;
};

struct RunAnalysis
{
    std::vector<BeepResult> beeps;
    RunMetrics metrics;
};

RunAnalysis analyzeScheduled(const Recording &rec, const std::vector<ScheduledBeep> &schedule,
                             const std::vector<float> &sound, int soundRate);

// Listen-only mode: time another app (EonTimer) without knowing its schedule.
// Beeps closer than 1.5 x the nominal interval are one group.
struct ListenEvent
{
    double ms = 0;
    int group = 0;
    int indexInGroup = 0;
    double score = 0;
    double snr = 0;
};

struct ListenMetrics
{
    int beeps = 0;
    int groups = 0;
    double meanIntervalMs = kNaN;
    double intervalSdMs = kNaN;
    double intervalMaxAbsErrMs = kNaN;   // vs the nominal interval
    double firstIntervalDeltaMs = kNaN;  // mean error of each group's first interval minus the others
};

struct ListenAnalysis
{
    std::vector<ListenEvent> events;
    std::vector<double> intervalErrors; // per interval inside groups: actual - nominal
    ListenMetrics metrics;
};

ListenAnalysis analyzeListen(const Recording &rec, double nominalIntervalMs, const std::vector<float> &sound,
                             int soundRate);
}
