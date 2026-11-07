/*
 * This file is part of PokeTimerPlus
 */

#include "AudioEngine.hpp"

#include <algorithm>
#include <cstdio>
#include <miniaudio.h>

struct AudioEngine::State
{
    ma_context context {};
    bool contextOk = false;
    ma_device device {};
    bool deviceOk = false;
    bool followsDefault = true;
    double bufferMs = 0;
    std::string details;
    AudioEngine *owner = nullptr;
};

namespace
{
    void dataCallback(ma_device *device, void *output, const void *, ma_uint32 frameCount)
    {
        auto *state = static_cast<AudioEngine::State *>(device->pUserData);
        state->owner->mixer().render(static_cast<float *>(output), int(frameCount));
    }
}

static void notificationCallback(const ma_device_notification *n);

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    close();
}

BeepMixer &AudioEngine::mixer()
{
    return mixer_;
}

const BeepMixer &AudioEngine::mixer() const
{
    return mixer_;
}

void AudioEngine::setEventCallback(EventFn fn)
{
    events_ = std::move(fn); // set before open(); called from the audio thread
}

void AudioEngine::notify(Event e)
{
    if (events_)
    {
        events_(e);
    }
}

std::vector<OutputDevice> AudioEngine::outputs()
{
    std::vector<OutputDevice> out;
    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS)
    {
        return out;
    }
    ma_device_info *playback = nullptr;
    ma_uint32 count = 0;
    if (ma_context_get_devices(&context, &playback, &count, nullptr, nullptr) == MA_SUCCESS)
    {
        for (ma_uint32 i = 0; i < count; i++)
        {
            out.push_back({ playback[i].name, playback[i].isDefault != 0 });
        }
    }
    ma_context_uninit(&context);
    return out;
}

bool AudioEngine::open(const std::string &deviceName, std::string *error)
{
    close();
    state_ = std::make_unique<State>();
    state_->owner = this;
    auto fail = [&](const std::string &msg) {
        if (error)
        {
            *error = msg;
        }
        close();
        return false;
    };

    if (ma_context_init(nullptr, 0, nullptr, &state_->context) != MA_SUCCESS)
    {
        return fail("no audio backend");
    }
    state_->contextOk = true;

    const ma_device_id *id = nullptr;
    ma_device_info *playback = nullptr;
    ma_uint32 count = 0;
    if (!deviceName.empty() && ma_context_get_devices(&state_->context, &playback, &count, nullptr, nullptr) == MA_SUCCESS)
    {
        for (ma_uint32 i = 0; i < count; i++)
        {
            if (deviceName == playback[i].name)
            {
                id = &playback[i].id;
                break;
            }
        }
        if (!id)
        {
            return fail("output \"" + deviceName + "\" not found");
        }
    }
    state_->followsDefault = id == nullptr;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = id;
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 0; // native rate: no resampling in the path
    config.performanceProfile = ma_performance_profile_low_latency;
    config.periodSizeInMilliseconds = 10;
    config.dataCallback = dataCallback;
    config.notificationCallback = notificationCallback;
    config.pUserData = state_.get();

    if (ma_device_init(&state_->context, &config, &state_->device) != MA_SUCCESS)
    {
        return fail("cannot open the output");
    }
    state_->deviceOk = true;

    const ma_device &d = state_->device;
    const double periodMs = 1000.0 * d.playback.internalPeriodSizeInFrames / std::max<ma_uint32>(1, d.playback.internalSampleRate);
    state_->bufferMs = periodMs * std::max<ma_uint32>(1, d.playback.internalPeriods);
    mixer_.configure(int(d.sampleRate), int(d.playback.channels), state_->bufferMs);

    char buf[160];
    std::snprintf(buf, sizeof buf, "%s, %u Hz, %.1f ms x %u", ma_get_backend_name(state_->context.backend), d.sampleRate, periodMs,
                  d.playback.internalPeriods);
    state_->details = buf;

    if (ma_device_start(&state_->device) != MA_SUCCESS)
    {
        return fail("cannot start the output");
    }
    return true;
}

void AudioEngine::close()
{
    if (!state_)
    {
        return;
    }
    if (state_->deviceOk)
    {
        ma_device_uninit(&state_->device);
    }
    if (state_->contextOk)
    {
        ma_context_uninit(&state_->context);
    }
    state_.reset();
}

bool AudioEngine::isOpen() const
{
    return state_ && state_->deviceOk;
}

std::string AudioEngine::deviceName() const
{
    if (!isOpen())
    {
        return {};
    }
    ma_device_info info;
    if (ma_device_get_info(&state_->device, ma_device_type_playback, &info) == MA_SUCCESS)
    {
        return info.name;
    }
    return state_->device.playback.name;
}

bool AudioEngine::followsDefault() const
{
    return state_ && state_->followsDefault;
}

std::string AudioEngine::details() const
{
    return state_ ? state_->details : std::string();
}

double AudioEngine::bufferMs() const
{
    return state_ ? state_->bufferMs : 0;
}

static void notificationCallback(const ma_device_notification *n)
{
    auto *state = static_cast<AudioEngine::State *>(n->pDevice->pUserData);
    AudioEngine::Event e;
    switch (n->type)
    {
    case ma_device_notification_type_started:
        e = AudioEngine::Event::Started;
        break;
    case ma_device_notification_type_stopped:
        e = AudioEngine::Event::Stopped;
        break;
    case ma_device_notification_type_rerouted:
        e = AudioEngine::Event::Rerouted;
        break;
    case ma_device_notification_type_interruption_began:
        e = AudioEngine::Event::InterruptionBegan;
        break;
    case ma_device_notification_type_interruption_ended:
        e = AudioEngine::Event::InterruptionEnded;
        break;
    default:
        return;
    }
    state->owner->notify(e);
}
