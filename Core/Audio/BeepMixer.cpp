/*
 * This file is part of PokeTimerPlus
 */

#include "BeepMixer.hpp"

#include "Wav.hpp"
#include <Core/Util/Clock.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    constexpr double WindowMs = 500; // lead is judged on the median of this many ms of callbacks
    constexpr double CorrectAboveMs = 1.0; // re-anchor when the lead moved more than this
    constexpr double SlipAboveMs = 5.0; // a correction this large counts as a slip (stall)
    constexpr double LateToleranceMs = 5.0; // a beep due up to this long ago still plays (at once)
    constexpr double QuietBeforeFixMs = 3000; // drift fixes only when no beep is due this soon
}

BeepMixer::BeepMixer(ClockFn clock) : clock_(std::move(clock))
{
}

double BeepMixer::now() const
{
    return clock_ ? clock_() : Clock::nowMs();
}

void BeepMixer::configure(int sampleRate, int channels, double outputLatencyMs)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rate_ = sampleRate;
    channels_ = std::max(1, channels);
    latencyMs_ = outputLatencyMs;
    sound_ = rawSound_.empty() ? std::vector<float>() : Audio::resample(rawSound_, rawRate_, rate_);
    items_.clear();
    cursor_ = 0;
    anchored_ = false;
    windowsSeen_ = 0;
    windowLeads_.clear();
    previousD_ = 0;
    slips_ = 0;
    corrected_ = 0;
}

void BeepMixer::setSound(const std::vector<float> &sound, int soundRate)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rawSound_ = sound;
    rawRate_ = soundRate;
    sound_ = Audio::resample(rawSound_, rawRate_, rate_);
}

void BeepMixer::setVolume(float volume)
{
    std::lock_guard<std::mutex> lock(mutex_);
    volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void BeepMixer::setSchedule(const std::vector<double> &ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t len = int64_t(sound_.size());
    // Keep what is sounding right now; replace everything else.
    std::vector<Item> next;
    for (const Item &it : items_)
    {
        if (it.started && it.start + len > cursor_)
        {
            next.push_back(it);
        }
    }
    for (double t : ms)
    {
        next.push_back({ t, 0, false });
    }
    items_ = std::move(next);
    convertLocked();
}

void BeepMixer::clearSchedule()
{
    setSchedule({});
}

void BeepMixer::convertLocked()
{
    if (!anchored_)
    {
        return; // converted at the first render
    }
    const int64_t tolerance = int64_t(LateToleranceMs * rate_ / 1000.0);
    std::vector<Item> kept;
    kept.reserve(items_.size());
    for (Item it : items_)
    {
        if (!it.started)
        {
            it.start = anchorFrame_ + std::llround((it.ms - anchorMs_) * rate_ / 1000.0);
            if (it.start < cursor_)
            {
                if (cursor_ - it.start > tolerance)
                {
                    continue; // its time has passed: drop rather than play late
                }
                it.start = cursor_;
            }
        }
        kept.push_back(it);
    }
    std::sort(kept.begin(), kept.end(), [](const Item &a, const Item &b) { return a.start < b.start; });
    items_ = std::move(kept);
}

void BeepMixer::trackLeadLocked(double lead, double t)
{
    lastLead_ = lead;
    windowLeads_.push_back(lead);
    if (t - windowStartMs_ < WindowMs)
    {
        return;
    }
    const double maxLead = *std::max_element(windowLeads_.begin(), windowLeads_.end());
    auto mid = windowLeads_.begin() + windowLeads_.size() / 2;
    std::nth_element(windowLeads_.begin(), mid, windowLeads_.end());
    const double median = *mid;
    windowLeads_.clear();
    windowStartMs_ = t;
    windowsSeen_++;

    if (windowsSeen_ == 1)
    {
        return; // start-up window: prefill bursts
    }
    if (windowsSeen_ == 2)
    {
        // The first callback anchored the stream, including however late that callback ran. The
        // least-late callback of this window shows that lateness (its lead is the largest): take it
        // out, so the anchor doesn't change from one launch to the next with scheduling noise.
        const double firstCallbackLate = maxLead - latencyMs_;
        anchorMs_ -= firstCallbackLate;
        baselineLead_ = median - firstCallbackLate;
        convertLocked();
        return;
    }
    // The lead dropped by d: frames now come out d ms later than the anchor says. Moving the anchor
    // d ms later puts the beeps that have not started back on time.
    // Corrected only when two windows in a row agree (same direction, both over 1 ms), so a burst
    // of late callbacks never moves the beeps; a real stall or drift persists and gets corrected
    // within a second. A correction over 5 ms counts as a stall.
    const double d = baselineLead_ - median;
    double correction = 0;
    if (std::abs(d) > CorrectAboveMs && std::abs(previousD_) > CorrectAboveMs && (d > 0) == (previousD_ > 0))
    {
        correction = std::abs(d) < std::abs(previousD_) ? d : previousD_;
        if (std::abs(correction) > SlipAboveMs)
        {
            slips_++;
        }
    }
    // Small drift fixes wait for a gap between countdowns, so the beeps inside a countdown keep
    // exact spacing (a stall is fixed at once: the beeps are already off).
    if (correction != 0 && std::abs(correction) <= SlipAboveMs)
    {
        const int64_t soon = cursor_ + int64_t(QuietBeforeFixMs * rate_ / 1000.0);
        const int64_t len = int64_t(sound_.size());
        for (const Item &it : items_)
        {
            if ((!it.started && it.start < soon) || (it.started && it.start + len > cursor_))
            {
                correction = 0;
                break;
            }
        }
    }
    previousD_ = correction != 0 ? d - correction : d;
    if (correction != 0)
    {
        anchorMs_ += correction;
        corrected_ = corrected_.load() + correction;
        convertLocked();
    }
}

void BeepMixer::render(float *out, int frames)
{
    std::memset(out, 0, sizeof(float) * size_t(frames) * size_t(channels_));
    std::lock_guard<std::mutex> lock(mutex_);

    const double t = now();
    const int64_t begin = cursor_;
    if (!anchored_)
    {
        // These frames come out once the output buffer has drained.
        anchorMs_ = t + latencyMs_;
        anchorFrame_ = begin;
        anchored_ = true;
        windowStartMs_ = t;
        convertLocked();
    }
    trackLeadLocked(anchorMs_ + double(begin - anchorFrame_) * 1000.0 / rate_ - t, t);

    const int64_t end = begin + frames;
    const int64_t len = int64_t(sound_.size());
    for (Item &it : items_)
    {
        if (it.start >= end)
        {
            break;
        }
        if (it.start + len <= begin)
        {
            continue;
        }
        it.started = true;
        const int64_t from = std::max(begin, it.start);
        const int64_t to = std::min(end, it.start + len);
        for (int64_t f = from; f < to; f++)
        {
            const float v = sound_[size_t(f - it.start)] * volume_;
            float *frame = out + size_t(f - begin) * size_t(channels_);
            for (int c = 0; c < channels_; c++)
            {
                frame[c] += v;
            }
        }
    }
    // Forget beeps that have finished.
    items_.erase(std::remove_if(items_.begin(), items_.end(), [&](const Item &it) { return it.started && it.start + len <= end; }),
                 items_.end());
    cursor_ = end;
}

bool BeepMixer::anchored() const
{
    return anchored_;
}

int BeepMixer::sampleRate() const
{
    return rate_;
}

int BeepMixer::slipCount() const
{
    return slips_.load();
}

double BeepMixer::correctedMs() const
{
    return corrected_.load();
}

double BeepMixer::leadMs() const
{
    return lastLead_.load();
}
