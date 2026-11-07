#include "MainWindow.h"

#include "Clock.h"
#include "Engines.h"
#include "Platform.h"

#include <QApplication>
#include <QAudioDevice>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <initializer_list>

using namespace beeplab;

namespace
{
const char *kModeNames[] = { "A: EonTimer way (QSoundEffect)", "B: Stream (Qt QAudioSink)",
                             "C: Stream (miniaudio)", "Listen only (time another app, e.g. EonTimer)" };
const char *kModeShort[] = { "A EonTimer way", "B Qt stream", "C miniaudio", "Listen" };

QString num(double v, int decimals = 2)
{
    return std::isnan(v) ? QStringLiteral("–") : QString::number(v, 'f', decimals);
}

QString csvNum(double v, int decimals = 3)
{
    return std::isnan(v) ? QString() : QString::number(v, 'f', decimals);
}

QString csvField(QString s)
{
    if (s.contains(',') || s.contains('"') || s.contains('\n'))
    {
        s.replace('"', QStringLiteral("\"\""));
        return '"' + s + '"';
    }
    return s;
}

QAudioDevice findDevice(const QList<QAudioDevice> &list, const QByteArray &id)
{
    for (const auto &d : list)
    {
        if (d.id() == id)
        {
            return d;
        }
    }
    return {};
}

// Green / orange / red background for a timing value in ms (translucent so dark mode stays readable).
QBrush grade(double ms, double good, double bad)
{
    if (std::isnan(ms))
    {
        return {};
    }
    double a = std::abs(ms);
    QColor c = a <= good ? QColor(40, 170, 80, 90) : (a <= bad ? QColor(230, 150, 30, 90) : QColor(220, 50, 50, 110));
    return QBrush(c);
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), devices_(new QMediaDevices(this)), timer_(new QTimer(this))
{
    setWindowTitle(QStringLiteral("PokeTimerPlus BeepLab (Phase 0)"));
    buildUi();
    refreshDevices();
    loadSettings();

    timer_->setInterval(100);
    connect(timer_, &QTimer::timeout, this, &MainWindow::tick);
    connect(&recorder_, &Recorder::levelChanged, this, [this](float peak) {
        level_->setValue(int(std::min(1.0f, peak) * 100));
    });
    connect(devices_, &QMediaDevices::audioOutputsChanged, this, [this] {
        log(QStringLiteral("Audio outputs changed"));
        if (run_.active)
        {
            run_.events << QStringLiteral("outputs changed");
        }
        refreshDevices();
    });
    connect(devices_, &QMediaDevices::audioInputsChanged, this, [this] {
        log(QStringLiteral("Audio inputs changed"));
        if (run_.active)
        {
            run_.events << QStringLiteral("inputs changed");
        }
        refreshDevices();
    });

    log(QStringLiteral("Results go to %1").arg(resultsDir()));
    log(QStringLiteral("Start with Test beep: it should say the beeps were heard."));
}

MainWindow::~MainWindow()
{
    saveSettings();
    cleanupRun();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    // ---- Setup
    auto *setupBox = new QGroupBox(QStringLiteral("Setup"));
    auto *setup = new QFormLayout(setupBox);
    engineBox_ = new QComboBox;
    for (const char *n : kModeNames)
    {
        engineBox_->addItem(QString::fromUtf8(n));
    }
    outputBox_ = new QComboBox;
    inputBox_ = new QComboBox;
    auto *refresh = new QPushButton(QStringLiteral("Refresh"));
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    auto *outRow = new QHBoxLayout;
    outRow->addWidget(outputBox_, 1);
    outRow->addWidget(refresh);
    soundBox_ = new QComboBox;
    soundBox_->addItems({ QStringLiteral("Beep"), QStringLiteral("Ding"), QStringLiteral("Pop"), QStringLiteral("Tick") });
    soundBox_->setToolTip(QStringLiteral("EonTimer's sounds. In Listen mode, pick the sound EonTimer is set to."));
    conditionBox_ = new QComboBox;
    conditionBox_->setEditable(true);
    conditionBox_->addItems({ QStringLiteral("Idle"), QStringLiteral("YouTube playing during the run"),
                              QStringLiteral("YouTube played just before"), QStringLiteral("Window hidden"),
                              QStringLiteral("Battery + Low Power Mode"), QStringLiteral("Bluetooth output"),
                              QStringLiteral("Long silence (30 s gaps)") });
    keepAwake_ = new QCheckBox(QStringLiteral("Keep the app awake during the run (no App Nap)"));
    keepAwake_->setToolTip(QStringLiteral("Off reproduces EonTimer, which does not ask macOS for this."));
    setup->addRow(QStringLiteral("Engine"), engineBox_);
    setup->addRow(QStringLiteral("Output (speaker)"), outRow);
    setup->addRow(QStringLiteral("Input (microphone)"), inputBox_);
    setup->addRow(QStringLiteral("Sound"), soundBox_);
    setup->addRow(QStringLiteral("Condition"), conditionBox_);
    setup->addRow(QString(), keepAwake_);

    // ---- Run
    auto *runBox = new QGroupBox(QStringLiteral("Run"));
    auto *runForm = new QFormLayout(runBox);
    auto spin = [](int min, int max, int value, const QString &suffix) {
        auto *s = new QSpinBox;
        s->setRange(min, max);
        s->setValue(value);
        s->setSuffix(suffix);
        return s;
    };
    groups_ = spin(1, 50, 6, QString());
    perGroup_ = spin(1, 20, 5, QString());
    interval_ = spin(100, 5000, 500, QStringLiteral(" ms"));
    gap_ = spin(1, 300, 8, QStringLiteral(" s"));
    first_ = spin(1, 60, 5, QStringLiteral(" s"));
    listen_ = spin(5, 600, 60, QStringLiteral(" s"));
    gap_->setToolTip(QStringLiteral("From one countdown's first beep to the next one's. Silence in between."));
    listen_->setToolTip(QStringLiteral("Listen mode only. Interval above = EonTimer's beep interval."));
    runForm->addRow(QStringLiteral("Countdowns"), groups_);
    runForm->addRow(QStringLiteral("Beeps per countdown"), perGroup_);
    runForm->addRow(QStringLiteral("Beep interval"), interval_);
    runForm->addRow(QStringLiteral("Countdown every"), gap_);
    runForm->addRow(QStringLiteral("First countdown after"), first_);
    runForm->addRow(QStringLiteral("Listen for"), listen_);

    resultsDir_ = new QLineEdit;
    auto *choose = new QPushButton(QStringLiteral("Choose…"));
    connect(choose, &QPushButton::clicked, this, [this] {
        QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("Results folder"), resultsDir());
        if (!d.isEmpty())
        {
            resultsDir_->setText(d);
        }
    });
    auto *open = new QPushButton(QStringLiteral("Open"));
    connect(open, &QPushButton::clicked, this, [this] {
        QDir().mkpath(resultsDir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(resultsDir()));
    });
    auto *dirRow = new QHBoxLayout;
    dirRow->addWidget(resultsDir_, 1);
    dirRow->addWidget(choose);
    dirRow->addWidget(open);
    keepRecordings_ = new QCheckBox(QStringLiteral("Keep the recordings (WAV, for re-analysis)"));
    keepRecordings_->setChecked(true);
    runForm->addRow(QStringLiteral("Results folder"), dirRow);
    runForm->addRow(QString(), keepRecordings_);

    auto *top = new QHBoxLayout;
    top->addWidget(setupBox, 3);
    top->addWidget(runBox, 2);
    root->addLayout(top);

    // ---- Controls
    auto *controls = new QHBoxLayout;
    testButton_ = new QPushButton(QStringLiteral("Test beep"));
    testButton_->setToolTip(QStringLiteral("3 beeps with the chosen engine; checks the microphone hears them."));
    startButton_ = new QPushButton(QStringLiteral("Start run"));
    startButton_->setDefault(true);
    stopButton_ = new QPushButton(QStringLiteral("Stop"));
    stopButton_->setEnabled(false);
    connect(testButton_, &QPushButton::clicked, this, [this] { startRun(true); });
    connect(startButton_, &QPushButton::clicked, this, [this] { startRun(false); });
    connect(stopButton_, &QPushButton::clicked, this, [this] { abortRun(QStringLiteral("stopped")); });
    level_ = new QProgressBar;
    level_->setRange(0, 100);
    level_->setFormat(QStringLiteral("mic %p%"));
    level_->setMaximumWidth(160);
    progress_ = new QProgressBar;
    progress_->setRange(0, 1000);
    progress_->setTextVisible(false);
    status_ = new QLabel(QStringLiteral("Ready"));
    controls->addWidget(testButton_);
    controls->addWidget(startButton_);
    controls->addWidget(stopButton_);
    controls->addWidget(level_);
    controls->addWidget(progress_, 1);
    root->addLayout(controls);
    root->addWidget(status_);

    // ---- Results + log
    table_ = new QTableWidget(0, 17);
    table_->setHorizontalHeaderLabels(
        { QStringLiteral("Time"), QStringLiteral("Engine"), QStringLiteral("Output"), QStringLiteral("Condition"),
          QStringLiteral("Awake"), QStringLiteral("Power"), QStringLiteral("Heard"), QStringLiteral("Latency"),
          QStringLiteral("Jitter σ"), QStringLiteral("Max |err|"), QStringLiteral("1st beep Δ"),
          QStringLiteral("Drift ppm"), QStringLiteral("Interval σ"), QStringLiteral("Interval max"),
          QStringLiteral("Fired late avg"), QStringLiteral("Fired late max"), QStringLiteral("Events") });
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setToolTip(QStringLiteral("All in ms unless noted. Jitter σ / Max |err|: what calibration cannot fix.\n"
                                      "1st beep Δ: how much later the first beep of each countdown is than the others.\n"
                                      "Latency: the constant part (calibrated away). Listen rows: interval stats only."));
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    auto *split = new QSplitter(Qt::Vertical);
    split->addWidget(table_);
    split->addWidget(log_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    setCentralWidget(central);
    resize(1180, 760);
}

void MainWindow::refreshDevices()
{
    auto fill = [](QComboBox *box, const QList<QAudioDevice> &list, const QAudioDevice &def) {
        QByteArray current = box->currentData().toByteArray();
        box->blockSignals(true);
        box->clear();
        for (const auto &d : list)
        {
            box->addItem(d.description() + (d.id() == def.id() ? QStringLiteral(" (default)") : QString()), d.id());
        }
        int idx = box->findData(current);
        if (idx < 0)
        {
            idx = box->findData(def.id());
        }
        box->setCurrentIndex(std::max(0, idx));
        box->blockSignals(false);
    };
    fill(outputBox_, QMediaDevices::audioOutputs(), QMediaDevices::defaultAudioOutput());
    fill(inputBox_, QMediaDevices::audioInputs(), QMediaDevices::defaultAudioInput());
}

void MainWindow::loadSettings()
{
    QSettings s;
    engineBox_->setCurrentIndex(s.value("engine", 0).toInt());
    int o = outputBox_->findData(s.value("output").toByteArray());
    if (o >= 0)
    {
        outputBox_->setCurrentIndex(o);
    }
    int i = inputBox_->findData(s.value("input").toByteArray());
    if (i >= 0)
    {
        inputBox_->setCurrentIndex(i);
    }
    soundBox_->setCurrentText(s.value("sound", "Beep").toString());
    conditionBox_->setCurrentText(s.value("condition", "Idle").toString());
    keepAwake_->setChecked(s.value("keepAwake", false).toBool());
    keepRecordings_->setChecked(s.value("keepRecordings", true).toBool());
    groups_->setValue(s.value("groups", 6).toInt());
    perGroup_->setValue(s.value("perGroup", 5).toInt());
    interval_->setValue(s.value("interval", 500).toInt());
    gap_->setValue(s.value("gap", 8).toInt());
    first_->setValue(s.value("first", 5).toInt());
    listen_->setValue(s.value("listen", 60).toInt());
    resultsDir_->setText(s.value("resultsDir", QStringLiteral(BEEPLAB_RESULTS_DIR)).toString());
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue("engine", engineBox_->currentIndex());
    s.setValue("output", outputBox_->currentData());
    s.setValue("input", inputBox_->currentData());
    s.setValue("sound", soundBox_->currentText());
    s.setValue("condition", conditionBox_->currentText());
    s.setValue("keepAwake", keepAwake_->isChecked());
    s.setValue("keepRecordings", keepRecordings_->isChecked());
    s.setValue("groups", groups_->value());
    s.setValue("perGroup", perGroup_->value());
    s.setValue("interval", interval_->value());
    s.setValue("gap", gap_->value());
    s.setValue("first", first_->value());
    s.setValue("listen", listen_->value());
    s.setValue("resultsDir", resultsDir_->text());
}

QString MainWindow::resultsDir() const
{
    QString d = resultsDir_->text().trimmed();
    return d.isEmpty() ? QStringLiteral(BEEPLAB_RESULTS_DIR) : d;
}

void MainWindow::log(const QString &text)
{
    log_->appendPlainText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz  ")) + text);
    if (autoMode_)
    {
        std::fprintf(stderr, "%s\n", text.toUtf8().constData());
    }
}

void MainWindow::setRunning(bool running)
{
    testButton_->setEnabled(!running);
    startButton_->setEnabled(!running);
    stopButton_->setEnabled(running);
    for (QWidget *w : std::initializer_list<QWidget *> { engineBox_, outputBox_, inputBox_, soundBox_, keepAwake_,
                                                          groups_, perGroup_, interval_, gap_, first_, listen_ })
    {
        w->setEnabled(!running);
    }
}

Sound MainWindow::loadSound(const QString &name) const
{
    QFile f(QStringLiteral(":/sounds/%1.wav").arg(name.toLower()));
    if (!f.open(QIODevice::ReadOnly))
    {
        return {};
    }
    QByteArray b = f.readAll();
    return parseWav(std::vector<unsigned char>(b.begin(), b.end()));
}

// ---------------------------------------------------------------- run lifecycle

void MainWindow::autoRun(const QList<int> &modes, const QString &dir)
{
    autoMode_ = true;
    autoQueue_ = modes;
    groups_->setValue(3);
    perGroup_->setValue(4);
    interval_->setValue(500);
    gap_->setValue(3);
    first_->setValue(1);
    listen_->setValue(14);
    if (!dir.isEmpty())
    {
        resultsDir_->setText(dir);
    }
    autoNext();
}

void MainWindow::autoNext()
{
    if (!autoMode_)
    {
        return;
    }
    QTimer::singleShot(500, this, [this] {
        if (autoQueue_.isEmpty())
        {
            QApplication::quit();
            return;
        }
        engineBox_->setCurrentIndex(autoQueue_.takeFirst());
        startRun(false);
    });
}

void MainWindow::startRun(bool test)
{
    if (run_.active)
    {
        return;
    }
    saveSettings();

    refreshDevices();
    QAudioDevice output = findDevice(QMediaDevices::audioOutputs(), outputBox_->currentData().toByteArray());
    QAudioDevice input = findDevice(QMediaDevices::audioInputs(), inputBox_->currentData().toByteArray());
    if (output.isNull())
    {
        output = QMediaDevices::defaultAudioOutput();
    }
    if (input.isNull())
    {
        input = QMediaDevices::defaultAudioInput();
    }
    const Mode mode = Mode(engineBox_->currentIndex());
    auto warn = [this](const QString &text) {
        log(text);
        if (autoMode_)
        {
            autoNext();
        }
        else
        {
            QMessageBox::warning(this, windowTitle(), text);
        }
    };
    if (input.isNull())
    {
        warn(QStringLiteral("No microphone found."));
        return;
    }
    if (mode != Listen && output.isNull())
    {
        warn(QStringLiteral("No output device found."));
        return;
    }

    run_ = RunContext();
    run_.active = true;
    run_.test = test;
    run_.mode = mode;
    run_.modeName = QString::fromUtf8(kModeShort[mode]);
    run_.output = mode == Listen ? QStringLiteral("(other app)") : output.description();
    run_.input = input.description();
    run_.sound = soundBox_->currentText();
    run_.condition = conditionBox_->currentText();
    run_.keepAwake = keepAwake_->isChecked();
    run_.lowPower = platform::lowPowerMode();
    run_.power = platform::powerSource() + (run_.lowPower ? QStringLiteral(" + Low Power") : QString());
    run_.groups = test ? 1 : groups_->value();
    run_.perGroup = test ? 3 : perGroup_->value();
    run_.intervalMs = interval_->value();
    run_.gapS = gap_->value();
    run_.firstS = test ? 1 : first_->value();
    run_.started = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    run_.soundData = loadSound(run_.sound);
    if (run_.soundData.samples.empty())
    {
        abortRun(QStringLiteral("cannot load the sound"));
        return;
    }
    if (!test && mode != Listen && run_.gapS * 1000 <= (run_.perGroup - 1) * run_.intervalMs + 300)
    {
        abortRun(QStringLiteral("countdowns overlap: raise 'Countdown every'"));
        return;
    }

    setRunning(true);
    log(QStringLiteral("%1 %2 | out: %3 | in: %4 | %5 | awake %6 | %7")
            .arg(test ? QStringLiteral("Test") : QStringLiteral("Run"), run_.modeName, run_.output, run_.input,
                 run_.condition, run_.keepAwake ? QStringLiteral("on") : QStringLiteral("off"), run_.power));

    QString error;
    if (!recorder_.start(input, &error))
    {
        abortRun(error);
        return;
    }
    if (run_.keepAwake)
    {
        platform::setKeepAwake(true);
    }
    run_.startMs = nowMs();

    if (mode == Listen)
    {
        run_.endMs = run_.startMs + listen_->value() * 1000.0;
        run_.scheduled = true;
        status_->setText(QStringLiteral("Listening… start the EonTimer countdowns now."));
        timer_->start();
        return;
    }

    Engine *engine = nullptr;
    switch (mode)
    {
    case EngineA:
        engine = new SoundEffectEngine(this);
        break;
    case EngineB:
        engine = new QtStreamEngine(this);
        break;
    default:
        engine = new MiniaudioEngine(this);
        break;
    }
    engine_ = engine;
    connect(engine, &Engine::notice, this, [this](const QString &text) {
        log(text);
        if (run_.active)
        {
            run_.events << text;
        }
    });
    const QString url = QStringLiteral("qrc:/sounds/%1.wav").arg(run_.sound.toLower());
    if (!engine->open(output, run_.soundData, url, &error))
    {
        abortRun(error);
        return;
    }
    run_.details = engine->details() + QStringLiteral(" | mic ") + recorder_.details();
    log(run_.details);

    // One second of warm-up, then the schedule (relative to now, like a timer start).
    status_->setText(test ? QStringLiteral("Test: 3 beeps…") : QStringLiteral("Running… keep the room quiet."));
    run_.endMs = run_.startMs + 1000 + run_.firstS * 1000.0 + (run_.groups - 1) * run_.gapS * 1000.0 +
                 (run_.perGroup - 1) * run_.intervalMs + 3000;
    QTimer::singleShot(1000, this, [this] {
        if (!run_.active || !engine_)
        {
            return;
        }
        const double base = nowMs() + run_.firstS * 1000.0;
        for (int g = 0; g < run_.groups; g++)
        {
            for (int i = 0; i < run_.perGroup; i++)
            {
                ScheduledBeep b;
                b.ms = base + g * run_.gapS * 1000.0 + i * run_.intervalMs;
                b.group = g;
                b.indexInGroup = i;
                run_.schedule.push_back(b);
            }
        }
        run_.endMs = run_.schedule.back().ms + 3000;
        run_.scheduled = true;
        engine_->play(run_.schedule);
    });
    timer_->start();
}

void MainWindow::tick()
{
    if (!run_.active)
    {
        return;
    }
    const double now = nowMs();
    const double total = std::max(1.0, run_.endMs - run_.startMs);
    progress_->setValue(int(1000 * std::clamp((now - run_.startMs) / total, 0.0, 1.0)));
    if (run_.scheduled && now >= run_.endMs)
    {
        finishRun();
    }
}

void MainWindow::finishRun()
{
    timer_->stop();
    Recording rec = recorder_.stop();
    if (engine_)
    {
        const QString notes = engine_->runNotes();
        if (!notes.isEmpty())
        {
            run_.events << notes;
            log(notes);
        }
        std::vector<double> triggers = engine_->triggerTimes();
        for (size_t i = 0; i < triggers.size() && i < run_.schedule.size(); i++)
        {
            run_.schedule[i].triggerMs = triggers[i];
        }
    }
    cleanupRun();
    setRunning(false);
    progress_->setValue(1000);
    level_->setValue(0);

    if (rec.samples.empty())
    {
        status_->setText(QStringLiteral("The microphone recorded nothing. Permission? (System Settings → Privacy → Microphone)"));
        log(status_->text());
        autoNext();
        return;
    }
    const double peak = [&] {
        float p = 0;
        for (float v : rec.samples)
        {
            p = std::max(p, std::abs(v));
        }
        return double(p);
    }();

    if (run_.mode == Listen)
    {
        ListenAnalysis a = analyzeListen(rec, run_.intervalMs, run_.soundData.samples, run_.soundData.sampleRate);
        status_->setText(QStringLiteral("Listen: %1 beeps in %2 countdowns. Interval σ %3 ms, worst error %4 ms.")
                             .arg(a.metrics.beeps)
                             .arg(a.metrics.groups)
                             .arg(num(a.metrics.intervalSdMs), num(a.metrics.intervalMaxAbsErrMs)));
        log(status_->text());
        addListenRow(a);
        saveListen(a, rec);
        autoNext();
        return;
    }

    RunAnalysis a = analyzeScheduled(rec, run_.schedule, run_.soundData.samples, run_.soundData.sampleRate);
    const auto &m = a.metrics;
    if (run_.test)
    {
        if (m.detected == 0)
        {
            status_->setText(QStringLiteral("Test: no beep heard (mic peak %1%). Raise the volume or move the mic closer.")
                                 .arg(int(peak * 100)));
        }
        else
        {
            status_->setText(QStringLiteral("Test: heard %1/%2 beeps, latency %3 ms, mic peak %4%. %5")
                                 .arg(m.detected)
                                 .arg(m.total)
                                 .arg(num(m.latencyMs, 1))
                                 .arg(int(peak * 100))
                                 .arg(m.detected == m.total ? QStringLiteral("Ready for runs.")
                                                            : QStringLiteral("Some missed: louder, or quieter room.")));
        }
        log(status_->text());
        return;
    }
    status_->setText(QStringLiteral("Done: heard %1/%2, jitter σ %3 ms, worst %4 ms, first beep Δ %5 ms.")
                         .arg(m.detected)
                         .arg(m.total)
                         .arg(num(m.jitterMs), num(m.maxAbsMs), num(m.firstBeepDeltaMs)));
    log(status_->text());
    if (peak > 0.99)
    {
        log(QStringLiteral("Warning: the recording clipped. Lower the volume a little."));
    }
    addScheduledRow(a);
    saveScheduled(a, rec);
    autoNext();
}

void MainWindow::abortRun(const QString &why)
{
    log(QStringLiteral("Run aborted: %1").arg(why));
    status_->setText(QStringLiteral("Aborted: %1").arg(why));
    timer_->stop();
    recorder_.stop();
    cleanupRun();
    setRunning(false);
    level_->setValue(0);
    progress_->setValue(0);
    autoNext();
}

void MainWindow::cleanupRun()
{
    if (engine_)
    {
        engine_->close();
        engine_->deleteLater();
        engine_ = nullptr;
    }
    if (run_.keepAwake)
    {
        platform::setKeepAwake(false);
    }
    run_.active = false;
}

// ---------------------------------------------------------------- results

void MainWindow::addScheduledRow(const RunAnalysis &a)
{
    const auto &m = a.metrics;
    int r = table_->rowCount();
    table_->insertRow(r);
    auto set = [&](int c, const QString &text, const QBrush &bg = {}) {
        auto *item = new QTableWidgetItem(text);
        if (bg.style() != Qt::NoBrush)
        {
            item->setBackground(bg);
        }
        item->setToolTip(run_.details);
        table_->setItem(r, c, item);
    };
    set(0, run_.started.mid(11));
    set(1, run_.modeName);
    set(2, run_.output);
    set(3, run_.condition);
    set(4, run_.keepAwake ? QStringLiteral("yes") : QStringLiteral("no"));
    set(5, run_.power);
    set(6, QStringLiteral("%1/%2").arg(m.detected).arg(m.total),
        m.detected == m.total ? QBrush() : grade(99, 0, 1));
    set(7, num(m.latencyMs, 1));
    set(8, num(m.jitterMs), grade(m.jitterMs, 1, 4));
    set(9, num(m.maxAbsMs), grade(m.maxAbsMs, 2, 8));
    set(10, num(m.firstBeepDeltaMs), grade(m.firstBeepDeltaMs, 2, 8));
    set(11, num(m.driftPpm, 0));
    table_->item(r, 11)->setToolTip(QStringLiteral("Output clock vs microphone clock. Microphone vs system clock: %1 ppm")
                                        .arg(num(m.micClockPpm, 0)));
    set(12, num(m.intervalSdMs), grade(m.intervalSdMs, 1, 4));
    set(13, num(m.intervalMaxAbsMs), grade(m.intervalMaxAbsMs, 2, 8));
    set(14, num(m.triggerLateMeanMs));
    set(15, num(m.triggerLateMaxMs));
    set(16, run_.events.isEmpty() ? QString() : run_.events.join(QStringLiteral("; ")));
    table_->scrollToBottom();
}

void MainWindow::addListenRow(const ListenAnalysis &a)
{
    const auto &m = a.metrics;
    int r = table_->rowCount();
    table_->insertRow(r);
    auto set = [&](int c, const QString &text, const QBrush &bg = {}) {
        auto *item = new QTableWidgetItem(text);
        if (bg.style() != Qt::NoBrush)
        {
            item->setBackground(bg);
        }
        table_->setItem(r, c, item);
    };
    set(0, run_.started.mid(11));
    set(1, run_.modeName);
    set(2, run_.output);
    set(3, run_.condition);
    set(4, run_.keepAwake ? QStringLiteral("yes") : QStringLiteral("no"));
    set(5, run_.power);
    set(6, QStringLiteral("%1 in %2").arg(m.beeps).arg(m.groups));
    set(10, num(m.firstIntervalDeltaMs), grade(m.firstIntervalDeltaMs, 2, 8));
    set(12, num(m.intervalSdMs), grade(m.intervalSdMs, 1, 4));
    set(13, num(m.intervalMaxAbsErrMs), grade(m.intervalMaxAbsErrMs, 2, 8));
    set(16, run_.events.join(QStringLiteral("; ")));
    table_->scrollToBottom();
}

QString MainWindow::runBaseName() const
{
    QString when = run_.started;
    when.replace(':', '-').replace(' ', '_');
    QString mode = run_.modeName;
    mode.replace(' ', '-');
    return QStringLiteral("%1_%2").arg(when, mode);
}

void MainWindow::saveScheduled(const RunAnalysis &a, const Recording &rec)
{
    QDir().mkpath(resultsDir());
    const QString base = QDir(resultsDir()).filePath(runBaseName());
    QFile f(base + QStringLiteral(".csv"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&f);
        out << "# " << run_.modeName << " | " << run_.output << " | " << run_.condition << " | " << run_.details
            << "\n";
        out << "beep,group,index_in_group,scheduled_ms,fired_late_ms,detected,offset_ms,residual_ms,score,snr\n";
        const double t0 = a.beeps.empty() ? 0 : a.beeps.front().scheduled.ms;
        for (size_t i = 0; i < a.beeps.size(); i++)
        {
            const auto &b = a.beeps[i];
            out << i << ',' << b.scheduled.group << ',' << b.scheduled.indexInGroup << ','
                << csvNum(b.scheduled.ms - t0) << ',' << csvNum(b.scheduled.triggerMs - b.scheduled.ms) << ','
                << (b.detected ? 1 : 0) << ',' << csvNum(b.offsetMs) << ',' << csvNum(b.residualMs) << ','
                << csvNum(b.score) << ',' << csvNum(b.snr, 1) << "\n";
        }
    }
    if (keepRecordings_->isChecked())
    {
        saveWav16((base + QStringLiteral(".wav")).toStdString(), rec.samples, rec.sampleRate);
        QFile s(base + QStringLiteral("_stamps.csv"));
        if (s.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&s);
            out << "frames,ms\n";
            for (const auto &st : rec.stamps)
            {
                out << st.frames << ',' << QString::number(st.ms, 'f', 4) << "\n";
            }
            out << "# schedule_ms\n";
            for (const auto &b : run_.schedule)
            {
                out << "# " << QString::number(b.ms, 'f', 4) << ',' << b.group << ',' << b.indexInGroup << "\n";
            }
        }
    }
    const auto &m = a.metrics;
    appendSummary({ run_.started, run_.modeName, run_.output, run_.input, run_.sound, run_.condition,
                    run_.keepAwake ? QStringLiteral("1") : QStringLiteral("0"), run_.power,
                    QString::number(run_.groups), QString::number(run_.perGroup), QString::number(run_.intervalMs),
                    QString::number(run_.gapS), QString::number(m.detected), QString::number(m.total),
                    csvNum(m.latencyMs), csvNum(m.jitterMs), csvNum(m.maxAbsMs), csvNum(m.firstBeepDeltaMs),
                    csvNum(m.driftPpm, 1), csvNum(m.intervalSdMs), csvNum(m.intervalMaxAbsMs),
                    csvNum(m.triggerLateMeanMs), csvNum(m.triggerLateMaxMs), QString(), csvNum(m.micClockPpm, 1),
                    run_.events.join(QStringLiteral("; ")), run_.details, runBaseName() });
}

void MainWindow::saveListen(const ListenAnalysis &a, const Recording &rec)
{
    QDir().mkpath(resultsDir());
    const QString base = QDir(resultsDir()).filePath(runBaseName());
    QFile f(base + QStringLiteral(".csv"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&f);
        out << "# Listen | nominal interval " << run_.intervalMs << " ms | " << run_.condition << "\n";
        out << "beep,group,index_in_group,time_ms,interval_error_ms,score,snr\n";
        const double t0 = a.events.empty() ? 0 : a.events.front().ms;
        double prev = kNaN;
        for (size_t i = 0; i < a.events.size(); i++)
        {
            const auto &e = a.events[i];
            double err = e.indexInGroup > 0 ? (e.ms - prev) - run_.intervalMs : kNaN;
            out << i << ',' << e.group << ',' << e.indexInGroup << ',' << csvNum(e.ms - t0) << ',' << csvNum(err)
                << ',' << csvNum(e.score) << ',' << csvNum(e.snr, 1) << "\n";
            prev = e.ms;
        }
    }
    if (keepRecordings_->isChecked())
    {
        saveWav16((base + QStringLiteral(".wav")).toStdString(), rec.samples, rec.sampleRate);
    }
    const auto &m = a.metrics;
    appendSummary({ run_.started, run_.modeName, run_.output, run_.input, run_.sound, run_.condition,
                    run_.keepAwake ? QStringLiteral("1") : QStringLiteral("0"), run_.power, QString::number(m.groups),
                    QString(), QString::number(run_.intervalMs), QString(), QString::number(m.beeps), QString(),
                    QString(), QString(), QString(), csvNum(m.firstIntervalDeltaMs), QString(), csvNum(m.intervalSdMs),
                    csvNum(m.intervalMaxAbsErrMs), QString(), QString(), csvNum(m.meanIntervalMs), QString(),
                    run_.events.join(QStringLiteral("; ")), recorder_.details(), runBaseName() });
}

void MainWindow::appendSummary(const QStringList &values)
{
    QDir().mkpath(resultsDir());
    QFile f(QDir(resultsDir()).filePath(QStringLiteral("summary.csv")));
    const bool fresh = !f.exists();
    if (!f.open(QIODevice::Append | QIODevice::Text))
    {
        log(QStringLiteral("Cannot write %1").arg(f.fileName()));
        return;
    }
    QTextStream out(&f);
    if (fresh)
    {
        out << "started,engine,output,input,sound,condition,keep_awake,power,groups,beeps_per_group,interval_ms,"
               "gap_s,detected,total,latency_ms,jitter_sd_ms,max_abs_ms,first_beep_delta_ms,drift_ppm,"
               "interval_sd_ms,interval_max_abs_ms,fired_late_mean_ms,fired_late_max_ms,listen_mean_interval_ms,"
               "mic_clock_ppm,events,details,files\n";
    }
    QStringList fields;
    for (const auto &v : values)
    {
        fields << csvField(v);
    }
    out << fields.join(',') << "\n";
    log(QStringLiteral("Saved %1").arg(runBaseName()));
}
