/*
 * This file is part of PokeTimerPlus
 */

#include "TimerController.hpp"

#include <Core/Timer/Timers.hpp>
#include <Core/Util/Clock.hpp>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <cmath>
#include <limits>

TimerController::TimerController(QObject *parent) : QObject(parent), timer_(new QTimer(this))
{
    timer_->setTimerType(Qt::PreciseTimer);
    timer_->setInterval(15);
    connect(timer_, &QTimer::timeout, this, &TimerController::tick);

    // Audio-thread notifications, handed to the GUI thread.
    QPointer<TimerController> self(this);
    engine_.setEventCallback([self](AudioEngine::Event e) {
        QMetaObject::invokeMethod(
            self.data(),
            [self, e] {
                if (!self)
                {
                    return;
                }
                if (e == AudioEngine::Event::Rerouted)
                {
                    emit self->outputChanged();
                }
                else if (e == AudioEngine::Event::Stopped && self->engine_.isOpen())
                {
                    emit self->outputProblem(tr("The output stopped. Pick it again in Settings if there's no sound."));
                }
            },
            Qt::QueuedConnection);
    });
}

TimerController::~TimerController()
{
    engine_.setEventCallback(nullptr);
    engine_.close();
}

bool TimerController::openOutput(const QString &deviceName, QString *error)
{
    const bool wasRunning = isRunning();
    std::string err;
    bool ok = engine_.open(deviceName.toStdString(), &err);
    if (!ok && !deviceName.isEmpty())
    {
        // The chosen output is gone (unplugged): fall back to the default rather than stay silent.
        std::string err2;
        ok = engine_.open({}, &err2);
    }
    if (error)
    {
        *error = QString::fromStdString(err);
    }
    if (wasRunning)
    {
        pushSchedule();
    }
    return ok;
}

QString TimerController::outputName() const
{
    return QString::fromStdString(engine_.deviceName());
}

QString TimerController::outputDetails() const
{
    return QString::fromStdString(engine_.details());
}

bool TimerController::followsDefault() const
{
    return engine_.followsDefault();
}

void TimerController::setSound(const Audio::Sound &sound)
{
    engine_.mixer().setSound(sound.samples, sound.sampleRate);
}

void TimerController::setBeeps(int count, int intervalMs)
{
    count_ = std::max(1, count);
    intervalMs_ = std::max(1, intervalMs);
}

void TimerController::setVisual(bool on)
{
    visual_ = on;
}

void TimerController::setAudio(bool on)
{
    audio_ = on;
    if (isRunning())
    {
        pushSchedule();
    }
}

void TimerController::start(const std::vector<double> &phases)
{
    // Like EonTimer 3: the clock starts on the click itself.
    startMs_ = Clock::nowMs();
    schedule_ = std::make_unique<Schedule>(phases, count_, intervalMs_);
    pushSchedule();
    timer_->start();
    tick();
}

void TimerController::startContinuous()
{
    startMs_ = Clock::nowMs();
    continuous_ = true;
    targets_.clear();
    schedule_.reset();
    pushSchedule();
    timer_->start();
    tick();
}

void TimerController::startExternal()
{
    startMs_ = Clock::nowMs();
    external_ = true;
    externalBeeps_.clear();
    schedule_.reset();
    pushSchedule();
    timer_->start();
    tick();
}

bool TimerController::isExternal() const
{
    return external_;
}

void TimerController::setExternalBeeps(const std::vector<double> &absoluteMs)
{
    if (!external_)
    {
        return;
    }
    externalBeeps_ = absoluteMs;
    pushSchedule();
}

bool TimerController::isContinuous() const
{
    return continuous_;
}

bool TimerController::addTarget(double ms)
{
    if (!continuous_)
    {
        return false;
    }
    targets_.push_back(ms);
    pushSchedule();
    return ms > Clock::nowMs() - startMs_;
}

void TimerController::removeLastTarget()
{
    if (continuous_ && !targets_.empty())
    {
        targets_.pop_back();
        pushSchedule();
    }
}

void TimerController::nudgeLastTarget(double deltaMs)
{
    if (continuous_ && !targets_.empty())
    {
        targets_.back() += deltaMs;
        pushSchedule();
    }
}

const std::vector<double> &TimerController::targets() const
{
    return targets_;
}

void TimerController::stop()
{
    timer_->stop();
    continuous_ = false;
    external_ = false;
    externalBeeps_.clear();
    targets_.clear();
    schedule_.reset();
    visualBeeps_.clear();
    engine_.mixer().clearSchedule();
}

bool TimerController::isRunning() const
{
    return schedule_ != nullptr || continuous_ || external_;
}

bool TimerController::setPhase(int phase, double durationMs)
{
    if (!schedule_)
    {
        return false;
    }
    bool ok = schedule_->setPhase(phase, durationMs, Clock::nowMs() - startMs_);
    pushSchedule();
    return ok;
}

void TimerController::pushSchedule()
{
    std::vector<double> times;
    visualBeeps_.clear();
    const double elapsed = Clock::nowMs() - startMs_;
    std::vector<Beep> beeps;
    if (schedule_)
    {
        beeps = schedule_->beepsAfter(elapsed - 5);
    }
    else if (external_)
    {
        for (double t : externalBeeps_)
        {
            if (t > startMs_ + elapsed - 5)
            {
                beeps.push_back(Beep { t - startMs_, 0, 0 });
            }
        }
    }
    else if (continuous_)
    {
        for (const Beep &b : beepsForTargets(targets_, count_, intervalMs_))
        {
            if (b.ms > elapsed - 5)
            {
                beeps.push_back(b);
            }
        }
    }
    for (const Beep &b : beeps)
    {
        times.push_back(startMs_ + b.ms);
        visualBeeps_.push_back(startMs_ + b.ms);
    }
    engine_.mixer().setSchedule(audio_ ? times : std::vector<double>());
}

void TimerController::testBeeps()
{
    if (isRunning())
    {
        return;
    }
    const double now = Clock::nowMs();
    std::vector<double> times;
    for (int k = count_ - 1; k >= 0; k--)
    {
        times.push_back(now + 300 + (count_ - 1 - k) * double(intervalMs_));
    }
    engine_.mixer().setSchedule(times);
}

int TimerController::slipCount() const
{
    return engine_.mixer().slipCount();
}

void TimerController::tick()
{
    if (!schedule_ && !continuous_ && !external_)
    {
        return;
    }
    const double now = Clock::nowMs();
    const double elapsed = now - startMs_;

    while (!visualBeeps_.empty() && visualBeeps_.front() <= now)
    {
        visualBeeps_.erase(visualBeeps_.begin());
        if (visual_)
        {
            emit beep();
        }
    }

    if (external_)
    {
        emit externalTick(now, elapsed);
        return;
    }

    if (continuous_)
    {
        double next = std::numeric_limits<double>::quiet_NaN();
        int left = 0;
        for (double t : targets_)
        {
            if (t > elapsed)
            {
                left++;
                if (std::isnan(next) || t - elapsed < next)
                {
                    next = t - elapsed;
                }
            }
        }
        emit continuousProgress(elapsed, next, left);
        return;
    }

    const auto &phases = schedule_->phases();
    const int phase = schedule_->phaseAt(elapsed);
    if (phase >= static_cast<int>(phases.size()))
    {
        // Let the last beep ring out before the controller forgets the schedule.
        timer_->stop();
        schedule_.reset();
        emit progress(static_cast<int>(phases.size()), 0, std::numeric_limits<double>::quiet_NaN(), elapsed);
        emit finished();
        return;
    }
    const double remaining = schedule_->phaseEnd(phase) - elapsed;
    const double next = phase + 1 < static_cast<int>(phases.size()) ? phases[phase + 1] : std::numeric_limits<double>::quiet_NaN();
    emit progress(phase, remaining, next, elapsed);
}
