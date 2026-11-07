#pragma once

#include "Analysis.h"
#include "Recorder.h"
#include "Wav.h"

#include <QMainWindow>
#include <QPointer>
#include <QStringList>

class Engine;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QMediaDevices;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Development aid (BEEPLAB_AUTORUN): short runs with these engine indexes, then quit.
    void autoRun(const QList<int> &modes, const QString &resultsDir);

private:
    enum Mode
    {
        EngineA,
        EngineB,
        EngineC,
        Listen
    };

    struct RunContext
    {
        bool active = false;
        bool test = false;
        Mode mode = EngineA;
        QString modeName, output, input, sound, condition, power, details, started;
        bool keepAwake = false;
        bool lowPower = false;
        int groups = 0, perGroup = 0, intervalMs = 0, gapS = 0, firstS = 0;
        double startMs = 0, endMs = 0;
        bool scheduled = false;
        beeplab::Sound soundData;
        std::vector<beeplab::ScheduledBeep> schedule;
        QStringList events;
    };

    void buildUi();
    void refreshDevices();
    void loadSettings();
    void saveSettings();
    void log(const QString &text);
    void setRunning(bool running);

    void startRun(bool test);
    void tick();
    void finishRun();
    void abortRun(const QString &why);
    void cleanupRun();
    void autoNext();

    void addScheduledRow(const beeplab::RunAnalysis &a);
    void addListenRow(const beeplab::ListenAnalysis &a);
    void saveScheduled(const beeplab::RunAnalysis &a, const beeplab::Recording &rec);
    void saveListen(const beeplab::ListenAnalysis &a, const beeplab::Recording &rec);
    void appendSummary(const QStringList &values);
    QString runBaseName() const;
    QString resultsDir() const;

    beeplab::Sound loadSound(const QString &name) const;

    // UI
    QComboBox *engineBox_, *outputBox_, *inputBox_, *soundBox_, *conditionBox_;
    QCheckBox *keepAwake_, *keepRecordings_;
    QSpinBox *groups_, *perGroup_, *interval_, *gap_, *first_, *listen_;
    QLineEdit *resultsDir_;
    QPushButton *testButton_, *startButton_, *stopButton_;
    QProgressBar *level_, *progress_;
    QLabel *status_;
    QTableWidget *table_;
    QPlainTextEdit *log_;

    QMediaDevices *devices_;
    Recorder recorder_;
    QPointer<Engine> engine_;
    QTimer *timer_;
    RunContext run_;
    QList<int> autoQueue_;
    bool autoMode_ = false;
};
