#include "BeepMixer.h"

#include "Clock.h"
#include "Wav.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace beeplab
{
void BeepMixer::configure(int sampleRate, int channels, const std::vector<float> &sound, int soundRate,
                          double outputLatencyMs)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rate_ = sampleRate;
    channels_ = std::max(1, channels);
    latencyMs_ = outputLatencyMs;
    sound_ = resample(sound, soundRate, sampleRate);
    anchored_ = false;
    cursor_ = 0;
    haveLead_ = false;
    slipCount_ = 0;
    slipMs_ = 0;
    starts_.clear();
    pendingMs_.clear();
}

void BeepMixer::setSchedule(const std::vector<double> &ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    pendingMs_ = ms;
    convertPending();
    // Count slips from now on only: start-up stalls before the run just shift the anchor (latency).
    haveLead_ = false;
    slipCount_ = 0;
    slipMs_ = 0;
}

void BeepMixer::convertPending()
{
    starts_.clear();
    if (!anchored_)
    {
        return; // converted at the first render
    }
    for (double t : pendingMs_)
    {
        starts_.push_back(anchorFrame_ + std::llround((t - anchorMs_) * rate_ / 1000.0));
    }
    std::sort(starts_.begin(), starts_.end());
}

void BeepMixer::render(float *out, int frames)
{
    std::memset(out, 0, sizeof(float) * size_t(frames) * channels_);
    std::lock_guard<std::mutex> lock(mutex_); // uncontended except when the schedule changes

    const int64_t begin = cursor_.load();
    if (!anchored_)
    {
        // The frames written now come out after the output buffer has drained.
        anchorMs_ = nowMs() + latencyMs_;
        anchorFrame_ = begin;
        anchored_ = true;
        convertPending();
    }

    // How far ahead of real time this block will be heard. Bursts make it wobble upward; a stall
    // makes it drop for good.
    const double lead = anchorMs_ + double(begin - anchorFrame_) * 1000.0 / rate_ - nowMs();
    if (!haveLead_)
    {
        minLeadMs_ = lead;
        haveLead_ = true;
    }
    else if (lead < minLeadMs_ - 10.0)
    {
        slipCount_++;
        slipMs_ = slipMs_.load() + (minLeadMs_ - lead);
        minLeadMs_ = lead;
    }
    else
    {
        minLeadMs_ = std::min(minLeadMs_, lead);
    }

    const int64_t end = begin + frames;
    const int64_t len = int64_t(sound_.size());
    for (int64_t start : starts_)
    {
        if (start >= end)
        {
            break;
        }
        if (start + len <= begin)
        {
            continue;
        }
        int64_t from = std::max(begin, start);
        int64_t to = std::min(end, start + len);
        for (int64_t f = from; f < to; f++)
        {
            float v = sound_[size_t(f - start)];
            float *frame = out + size_t(f - begin) * channels_;
            for (int c = 0; c < channels_; c++)
            {
                frame[c] += v;
            }
        }
    }
    cursor_ = end;
}
}
