/*
 * This file is part of PokeTimerPlus
 * Runs a timer: owns the audio stream (opened at start-up, kept open), turns a Schedule into beep
 * times for the mixer, and reports progress to the window.
 */

#ifndef TIMERCONTROLLER_HPP
#define TIMERCONTROLLER_HPP

#include <Core/Audio/AudioEngine.hpp>
#include <Core/Audio/Wav.hpp>
#include <Core/Timer/Schedule.hpp>
#include <QObject>
#include <memory>

class QTimer;

class TimerController : public QObject
{
    Q_OBJECT
public:
    explicit TimerController(QObject *parent = nullptr);
    ~TimerController() override;

    // Output. Empty name = system default.
    bool openOutput(const QString &deviceName, QString *error = nullptr);
    QString outputName() const; // device actually playing
    QString outputDetails() const;
    bool followsDefault() const;

    void setSound(const Audio::Sound &sound);
    void setBeeps(int count, int intervalMs);
    void setVisual(bool on);
    void setAudio(bool on);

    // Timer
    void start(const std::vector<double> &phases);
    void stop();
    bool isRunning() const;
    // Flowing target: change a phase while running (Gen 3 Variable Target)
    bool setPhase(int phase, double durationMs);

    // Continuous timer (FlowTimer-style): runs until stopped, targets (ms from start) added on the fly
    void startContinuous();
    bool isContinuous() const;
    bool addTarget(double ms); // false if already past
    void removeLastTarget();
    void nudgeLastTarget(double deltaMs);
    const std::vector<double> &targets() const;

    // Plays a short countdown now (settings "Test")
    void testBeeps();

    int slipCount() const;

signals:
    // phase = index of the running phase; remaining = ms left in it; next = length of the next one
    // (Infinity if open, NaN if none); elapsed = ms since start
    void progress(int phase, double remaining, double next, double elapsed);
    // continuous: time to the next target (NaN if none left), targets left
    void continuousProgress(double elapsed, double toNext, int targetsLeft);
    void beep(); // for the visual cue
    void finished();
    void outputChanged(); // the system default output changed under us
    void outputProblem(const QString &text);

private:
    void tick();
    void pushSchedule();

    AudioEngine engine_;
    std::unique_ptr<Schedule> schedule_;
    bool continuous_ = false;
    std::vector<double> targets_; // continuous targets, ms from start
    QTimer *timer_;
    double startMs_ = 0;
    int count_ = 6;
    int intervalMs_ = 500;
    bool visual_ = true;
    bool audio_ = true;
    std::vector<double> visualBeeps_; // absolute ms of beeps not flashed yet
};

#endif // TIMERCONTROLLER_HPP
