// Engine C: miniaudio playback device (CoreAudio on macOS), low-latency profile, fed by BeepMixer.
#include "Engines.h"

#include "miniaudio.h"

#include <QMetaObject>
#include <QPointer>

#include <cstring>
#include <algorithm>

struct MiniaudioState
{
    ma_context context {};
    bool contextOk = false;
    ma_device device {};
    bool deviceOk = false;
    beeplab::BeepMixer *mixer = nullptr;
    QPointer<MiniaudioEngine> engine;
};

namespace
{
void dataCallback(ma_device *device, void *output, const void *, ma_uint32 frameCount)
{
    auto *state = static_cast<MiniaudioState *>(device->pUserData);
    state->mixer->render(static_cast<float *>(output), int(frameCount));
}

void notificationCallback(const ma_device_notification *n)
{
    auto *state = static_cast<MiniaudioState *>(n->pDevice->pUserData);
    const char *what = "unknown";
    switch (n->type)
    {
    case ma_device_notification_type_started:
        what = "started";
        break;
    case ma_device_notification_type_stopped:
        what = "stopped";
        break;
    case ma_device_notification_type_rerouted:
        what = "rerouted (default output changed)";
        break;
    case ma_device_notification_type_interruption_began:
        what = "interruption began";
        break;
    case ma_device_notification_type_interruption_ended:
        what = "interruption ended";
        break;
    case ma_device_notification_type_unlocked:
        what = "unlocked";
        break;
    }
    QString text = QStringLiteral("miniaudio: %1").arg(QString::fromLatin1(what));
    QPointer<MiniaudioEngine> engine = state->engine;
    if (engine)
    {
        QMetaObject::invokeMethod(
            engine.data(),
            [engine, text] {
                if (engine)
                {
                    emit engine->notice(text);
                }
            },
            Qt::QueuedConnection);
    }
}
}

MiniaudioEngine::MiniaudioEngine(QObject *parent) : Engine(parent)
{
}

MiniaudioEngine::~MiniaudioEngine()
{
    close();
}

bool MiniaudioEngine::open(const QAudioDevice &output, const beeplab::Sound &sound, const QString &, QString *error)
{
    close();
    state_ = std::make_unique<MiniaudioState>();
    state_->mixer = &mixer_;
    state_->engine = this;

    auto fail = [&](const QString &msg) {
        if (error)
        {
            *error = msg;
        }
        close();
        return false;
    };

    if (ma_context_init(nullptr, 0, nullptr, &state_->context) != MA_SUCCESS)
    {
        return fail(QStringLiteral("miniaudio: no audio backend"));
    }
    state_->contextOk = true;

    // Same physical device as the one picked in the UI, matched by name; default otherwise.
    ma_device_info *playback = nullptr;
    ma_uint32 playbackCount = 0;
    const ma_device_id *id = nullptr;
    QString matched = QStringLiteral("default");
    if (ma_context_get_devices(&state_->context, &playback, &playbackCount, nullptr, nullptr) == MA_SUCCESS)
    {
        const QString wanted = output.description();
        for (ma_uint32 i = 0; i < playbackCount; i++)
        {
            if (QString::fromUtf8(playback[i].name) == wanted)
            {
                id = &playback[i].id;
                matched = wanted;
                break;
            }
        }
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = id;
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 0; // device's native rate: no resampling in the path
    config.performanceProfile = ma_performance_profile_low_latency;
    config.periodSizeInMilliseconds = 10;
    config.dataCallback = dataCallback;
    config.notificationCallback = notificationCallback;
    config.pUserData = state_.get();

    if (ma_device_init(&state_->context, &config, &state_->device) != MA_SUCCESS)
    {
        return fail(QStringLiteral("miniaudio: cannot open the output device"));
    }
    state_->deviceOk = true;

    const ma_device &d = state_->device;
    const double periodMs = 1000.0 * d.playback.internalPeriodSizeInFrames / std::max<ma_uint32>(1, d.playback.internalSampleRate);
    const double bufferMs = periodMs * std::max<ma_uint32>(1, d.playback.internalPeriods);
    mixer_.configure(int(d.sampleRate), int(d.playback.channels), sound.samples, sound.sampleRate, bufferMs);

    if (ma_device_start(&state_->device) != MA_SUCCESS)
    {
        return fail(QStringLiteral("miniaudio: cannot start the device"));
    }
    details_ = QStringLiteral("%1, %2 Hz, period %3 ms x %4, device %5")
                   .arg(QString::fromUtf8(ma_get_backend_name(state_->context.backend)))
                   .arg(d.sampleRate)
                   .arg(periodMs, 0, 'f', 1)
                   .arg(d.playback.internalPeriods)
                   .arg(matched);
    return true;
}

void MiniaudioEngine::play(const std::vector<beeplab::ScheduledBeep> &schedule)
{
    std::vector<double> ms;
    for (const auto &s : schedule)
    {
        ms.push_back(s.ms);
    }
    mixer_.setSchedule(ms);
    if (!mixer_.anchored())
    {
        emit notice(QStringLiteral("stream not running yet when the schedule was set"));
    }
}

void MiniaudioEngine::close()
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
