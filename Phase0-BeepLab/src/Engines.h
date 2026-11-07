// The three ways of making beeps that Phase 0 compares.
//  A. EonTimer's way: a thread waits for each beep, then the main thread starts a QSoundEffect.
//  B. Stream via Qt (QAudioSink): one open stream, beeps written at sample positions (BeepMixer).
//  C. Stream via miniaudio: same mixer, CoreAudio driven directly with a small period.
#pragma once

#include "Analysis.h"
#include "BeepMixer.h"
#include "Wav.h"

#include <QAudioDevice>
#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

class QSoundEffect;
class QThread;

class Engine : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    ~Engine() override = default;

    virtual QString name() const = 0;
    // Opens the output (streams start running immediately, silent). soundUrl is for QSoundEffect.
    virtual bool open(const QAudioDevice &output, const beeplab::Sound &sound, const QString &soundUrl,
                      QString *error) = 0;
    // Beeps must be heard at these system times (Clock.h).
    virtual void play(const std::vector<beeplab::ScheduledBeep> &schedule) = 0;
    virtual void close() = 0;
    // Per beep, when the engine fired it (NaN when the engine schedules ahead instead of firing).
    virtual std::vector<double> triggerTimes() const { return {}; }
    virtual QString details() const { return {}; }
    // Anything to add to the run's events at the end (stream slips).
    virtual QString runNotes() const { return {}; }

signals:
    void notice(const QString &text); // device events, underruns, errors
};

// ---------------------------------------------------------------- A
class SoundEffectEngine : public Engine
{
    Q_OBJECT
public:
    explicit SoundEffectEngine(QObject *parent = nullptr);
    ~SoundEffectEngine() override;
    QString name() const override { return QStringLiteral("A: EonTimer way (QSoundEffect)"); }
    bool open(const QAudioDevice &output, const beeplab::Sound &sound, const QString &soundUrl,
              QString *error) override;
    void play(const std::vector<beeplab::ScheduledBeep> &schedule) override;
    void close() override;
    std::vector<double> triggerTimes() const override { return triggers_; }
    QString details() const override;

private:
    QSoundEffect *effect_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_ { false };
    std::vector<double> triggers_;
};

// ---------------------------------------------------------------- B
class QtStreamWorker;
class QtStreamEngine : public Engine
{
    Q_OBJECT
public:
    explicit QtStreamEngine(QObject *parent = nullptr);
    ~QtStreamEngine() override;
    QString name() const override { return QStringLiteral("B: Stream (Qt QAudioSink)"); }
    QString runNotes() const override
    {
        if (!mixer_.slipCount())
        {
            return {};
        }
        return QStringLiteral("stream slips: %1 (%2 ms)").arg(mixer_.slipCount()).arg(mixer_.slipMs(), 0, 'f', 1);
    }
    bool open(const QAudioDevice &output, const beeplab::Sound &sound, const QString &soundUrl,
              QString *error) override;
    void play(const std::vector<beeplab::ScheduledBeep> &schedule) override;
    void close() override;
    QString details() const override { return details_; }

private:
    beeplab::BeepMixer mixer_;
    QThread *thread_ = nullptr;
    QtStreamWorker *worker_ = nullptr;
    QString details_;
};

// ---------------------------------------------------------------- C
struct MiniaudioState;
class MiniaudioEngine : public Engine
{
    Q_OBJECT
public:
    explicit MiniaudioEngine(QObject *parent = nullptr);
    ~MiniaudioEngine() override;
    QString name() const override { return QStringLiteral("C: Stream (miniaudio)"); }
    QString runNotes() const override
    {
        if (!mixer_.slipCount())
        {
            return {};
        }
        return QStringLiteral("stream slips: %1 (%2 ms)").arg(mixer_.slipCount()).arg(mixer_.slipMs(), 0, 'f', 1);
    }
    bool open(const QAudioDevice &output, const beeplab::Sound &sound, const QString &soundUrl,
              QString *error) override;
    void play(const std::vector<beeplab::ScheduledBeep> &schedule) override;
    void close() override;
    QString details() const override { return details_; }

private:
    beeplab::BeepMixer mixer_;
    std::unique_ptr<MiniaudioState> state_;
    QString details_;
};
