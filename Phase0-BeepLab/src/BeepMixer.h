// The core of the new engine (FlowTimer's idea): an audio stream that never stops, into which beeps
// are written at exact sample positions. Timing is then set by the sound card's sample clock, not by
// a thread waking up on time. Pure C++, used by both stream engines.
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace beeplab
{
class BeepMixer
{
public:
    // sound: mono samples at soundRate. Resampled to the stream rate.
    void configure(int sampleRate, int channels, const std::vector<float> &sound, int soundRate,
                   double outputLatencyMs);

    // Called from the audio callback. Writes interleaved float frames.
    void render(float *out, int frames);

    // System times (ms, Clock.h) at which each beep must be heard. Safe to call while running;
    // beeps already started keep playing. This is what "flowing" targets will use later.
    void setSchedule(const std::vector<double> &ms);

    bool anchored() const { return anchored_.load(); }
    int sampleRate() const { return rate_; }
    int channels() const { return channels_; }
    int64_t framesRendered() const { return cursor_.load(); }

    // Stream slips: the output stalled (underrun, device hiccup) so every later frame comes out late.
    // Seen as a sudden drop (> 10 ms) of how far ahead of real time the stream is being rendered.
    int slipCount() const { return slipCount_.load(); }
    double slipMs() const { return slipMs_.load(); }

private:
    void convertPending(); // under lock

    int rate_ = 48000;
    int channels_ = 2;
    double latencyMs_ = 0;
    std::vector<float> sound_;

    std::mutex mutex_;
    std::vector<double> pendingMs_; // schedule in system time, kept so it can be (re)converted
    std::vector<int64_t> starts_;   // schedule in stream frames

    // Anchor: stream frame anchorFrame_ is heard at system time anchorMs_ (+ constant unknown latency).
    std::atomic<bool> anchored_ { false };
    double anchorMs_ = 0;
    int64_t anchorFrame_ = 0;
    std::atomic<int64_t> cursor_ { 0 };
    double minLeadMs_ = 0;
    bool haveLead_ = false;
    std::atomic<int> slipCount_ { 0 };
    std::atomic<double> slipMs_ { 0 };
};
}
