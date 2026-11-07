// Engine A reproduces EonTimer 3.0.0-rc.8 (eon_timer/phase_runner.py + action/sound_manager.py):
// a worker thread sleeps in slices of at most refresh_interval (8 ms by default), spin-waits the
// last 2 ms, then signals the main thread, which calls QSoundEffect::play().
#include "Engines.h"

#include "Clock.h"

#include <QMetaObject>
#include <QSoundEffect>
#include <QUrl>

#include <chrono>
#include <cmath>
#include <algorithm>

namespace
{
constexpr double kRefreshIntervalMs = 8.0; // EonTimer TimerSettingsModel.refresh_interval default
constexpr double kSpinWaitMs = 2.0;        // EonTimer _SPINWAIT_MS
}

SoundEffectEngine::SoundEffectEngine(QObject *parent) : Engine(parent)
{
}

SoundEffectEngine::~SoundEffectEngine()
{
    close();
}

bool SoundEffectEngine::open(const QAudioDevice &output, const beeplab::Sound &, const QString &soundUrl,
                             QString *error)
{
    close();
    effect_ = new QSoundEffect(output, this);
    effect_->setSource(QUrl(soundUrl));
    effect_->setVolume(1.0);
    connect(effect_, &QSoundEffect::statusChanged, this, [this] {
        if (effect_ && effect_->status() == QSoundEffect::Error)
        {
            emit notice(QStringLiteral("QSoundEffect: error loading the sound"));
        }
    });
    if (effect_->status() == QSoundEffect::Error)
    {
        if (error)
        {
            *error = QStringLiteral("QSoundEffect could not load %1").arg(soundUrl);
        }
        return false;
    }
    return true;
}

void SoundEffectEngine::play(const std::vector<beeplab::ScheduledBeep> &schedule)
{
    if (!effect_)
    {
        return;
    }
    if (effect_->status() != QSoundEffect::Ready)
    {
        emit notice(QStringLiteral("QSoundEffect not ready yet (status %1)").arg(int(effect_->status())));
    }
    triggers_.assign(schedule.size(), beeplab::kNaN);
    std::vector<double> times;
    for (const auto &s : schedule)
    {
        times.push_back(s.ms);
    }

    running_ = true;
    thread_ = std::thread([this, times] {
        for (size_t i = 0; i < times.size() && running_; i++)
        {
            for (;;)
            {
                double remaining = times[i] - beeplab::nowMs();
                if (remaining <= 0 || !running_)
                {
                    break;
                }
                if (remaining > kSpinWaitMs)
                {
                    double sleepMs = std::min(kRefreshIntervalMs, remaining - kSpinWaitMs);
                    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(sleepMs));
                }
            }
            if (!running_)
            {
                break;
            }
            // Like EonTimer's action_triggered signal: queued to the main thread, which plays.
            QMetaObject::invokeMethod(
                this,
                [this, i] {
                    if (effect_ && i < triggers_.size())
                    {
                        triggers_[i] = beeplab::nowMs();
                        effect_->play();
                    }
                },
                Qt::QueuedConnection);
        }
    });
}

void SoundEffectEngine::close()
{
    running_ = false;
    if (thread_.joinable())
    {
        thread_.join();
    }
    if (effect_)
    {
        effect_->stop();
        effect_->deleteLater();
        effect_ = nullptr;
    }
}

QString SoundEffectEngine::details() const
{
    return QStringLiteral("refresh %1 ms, spin %2 ms").arg(kRefreshIntervalMs).arg(kSpinWaitMs);
}
