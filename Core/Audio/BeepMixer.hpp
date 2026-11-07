/*
 * This file is part of PokeTimerPlus
 * The audio engine's core (FlowTimer's idea, measured in Phase 0): a stream that never stops, into
 * which beeps are written at exact sample positions. Timing then comes from the sound card's sample
 * clock instead of a thread waking up on time.
 *
 * On top of BeepLab's version it keeps the stream on time against the system clock (both checked
 * on the median lead of 500 ms windows, and only acted on when two windows in a row agree):
 *  - a stall (underrun, device hiccup) is seen as a lasting drop of the render lead and corrected
 *    within a second (Phase 0 saw these on a virtual device);
 *  - slow drift between the sound card and the system clock is corrected when it exceeds 1 ms
 *    (Mystic Timer does the same with a 2 ms limit).
 * Beeps that already started are never moved or cut.
 */

#ifndef BEEPMIXER_HPP
#define BEEPMIXER_HPP

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

class BeepMixer
{
public:
    using ClockFn = std::function<double()>; // system time in ms

    explicit BeepMixer(ClockFn clock = nullptr);

    // sound: mono samples at soundRate (resampled to the stream rate). outputLatencyMs: buffer estimate.
    void configure(int sampleRate, int channels, double outputLatencyMs);
    void setSound(const std::vector<float> &sound, int soundRate);
    void setVolume(float volume);

    // Audio callback: writes interleaved float frames.
    void render(float *out, int frames);

    // System times (ms) at which beeps must be heard. Replaces the previous schedule; beeps whose
    // start is already past are dropped, beeps currently sounding finish. Safe while running.
    void setSchedule(const std::vector<double> &ms);
    void clearSchedule();

    bool anchored() const;
    int sampleRate() const;
    int slipCount() const;
    double correctedMs() const; // total anchor correction so far (slips + drift)
    double leadMs() const; // last render lead (how far ahead of real time the stream is rendered)

private:
    struct Item
    {
        double ms; // when it must be heard (system time)
        int64_t start; // stream frame
        bool started;
    };

    void convertLocked(); // ms -> frames for beeps that have not started
    void trackLeadLocked(double lead, double now);
    double now() const;

    ClockFn clock_;
    int rate_ = 48000;
    int channels_ = 2;
    double latencyMs_ = 0;
    float volume_ = 1.0f;
    std::vector<float> sound_;
    std::vector<float> rawSound_;
    int rawRate_ = 0;

    std::mutex mutex_;
    std::vector<Item> items_;
    int64_t cursor_ = 0;

    std::atomic<bool> anchored_ { false };
    double anchorMs_ = 0;
    int64_t anchorFrame_ = 0;

    // lead tracking: median lead per 500 ms window, compared with the second window's median
    double baselineLead_ = 0;
    double previousD_ = 0;
    int windowsSeen_ = 0;
    double windowStartMs_ = 0;
    std::vector<double> windowLeads_;

    std::atomic<int> slips_ { 0 };
    std::atomic<double> corrected_ { 0 };
    std::atomic<double> lastLead_ { 0 };
};

#endif // BEEPMIXER_HPP
