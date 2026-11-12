/*
 * This file is part of PokeTimerPlus
 */

#include "MainWindow.hpp"

#include "TimerController.hpp"
#include "TimerDisplay.hpp"
#include "TimerPanels.hpp"
#include "XYTidPanel.hpp"
#include <AppVersion.hpp>
#include <Core/Gen6/XYTid.hpp>
#include <Core/Timer/FrameRate.hpp>
#include <Core/Util/Clock.hpp>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyleHints>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

namespace
{
    void applyTheme(const QString &theme)
    {
        // Same choice as PokeFinder (auto / dark / light on the Fusion style).
        auto *hints = QGuiApplication::styleHints();
        if (theme == QLatin1String("dark"))
        {
            hints->setColorScheme(Qt::ColorScheme::Dark);
        }
        else if (theme == QLatin1String("light"))
        {
            hints->setColorScheme(Qt::ColorScheme::Light);
        }
        else
        {
            hints->unsetColorScheme();
        }
    }

    QString number(double v)
    {
        return QLocale::c().toString(v, 'f', std::abs(v - std::round(v)) < 1e-9 ? 0 : 2);
    }
}

QString MainWindow::storePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("calibrations.json"));
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), controller_(new TimerController(this))
{
    setWindowTitle(QStringLiteral("PokeTimerPlus %1").arg(QStringLiteral(POKETIMERPLUS_VERSION)));
    settings_ = AppSettings::load();
    applyTheme(settings_.theme);

    std::string error;
    store_ = CalibrationFile::load(storePath().toStdString(), &error);
    if (!error.empty())
    {
        QMessageBox::warning(this, windowTitle(),
                             tr("The saved calibrations could not be read (%1). Nothing is overwritten until you save one.")
                                 .arg(QString::fromStdString(error)));
    }

    buildUi();

    // The stream opens now and stays open: the device never idles between countdowns.
    QString openError;
    if (!controller_->openOutput(settings_.output, &openError) && !openError.isEmpty())
    {
        statusBar()->showMessage(tr("Audio: %1").arg(openError), 10000);
    }
    applySettings();
    connect(controller_, &TimerController::outputChanged, this, &MainWindow::outputChanged);
    connect(controller_, &TimerController::outputProblem, this, [this](const QString &t) { statusBar()->showMessage(t, 10000); });
    connect(controller_, &TimerController::progress, this,
            [this](int phase, double remaining, double next, double) { display_->showProgress(phase, remaining, next); });
    connect(controller_, &TimerController::continuousProgress, display_, &TimerDisplay::showContinuous);
    connect(controller_, &TimerController::beep, display_, &TimerDisplay::flash);
    connect(controller_, &TimerController::finished, this, &MainWindow::finished);
    connect(controller_, &TimerController::externalTick, this, [this](double now, double) {
        if (XYTidPanel *xy = xyPanel())
        {
            xy->tick(now);
        }
    });

    QSettings s;
    restoreGeometry(s.value("geometry").toByteArray());
    const int consoleIndex = std::clamp(s.value("console", 0).toInt(), 0, int(RngTypes::consoles().size()) - 1);
    consoles_->setCurrentIndex(consoleIndex);
    selectConsole(consoleIndex);
    outputChanged();
    listen();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *settingsAction
        = menuBar()->addMenu(tr("&File"))->addAction(tr("Settings…"), QKeySequence::Preferences, this, &MainWindow::openSettings);
    settingsAction->setMenuRole(QAction::PreferencesRole);
    auto *about = menuBar()->addMenu(tr("&Help"))->addAction(tr("About PokeTimerPlus"));
    about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("About PokeTimerPlus"),
                           tr("<b>PokeTimerPlus %1</b><br>All-in-one RNG timer.<br><br>Timer maths ported from EonTimer by DasAmpharos "
                              "(MIT). Continuous targets after FlowTimer by stringflow. Audio by miniaudio (public domain). Beep sounds "
                              "from EonTimer.<br><br>XY TID: method and timeline values by BZL, from 6genTidSearch, used with his "
                              "permission (なるほどえたきちのブログじゃねーの, "
                              "<a href=\"https://blog.bzl-web.com/entry/2020/08/31/202306\">XY</a>, "
                              "<a href=\"https://blog.bzl-web.com/entry/2020/08/24/225424\">ORAS</a>).")
                               .arg(QStringLiteral(POKETIMERPLUS_VERSION)));
    });

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    consoles_ = new QTabBar;
    consoles_->setExpanding(false);
    consoles_->setDocumentMode(true);
    for (const auto &c : RngTypes::consoles())
    {
        consoles_->addTab(QString::fromStdString(c.name));
    }
    root->addWidget(consoles_);

    auto *main = new QHBoxLayout;
    root->addLayout(main, 1);

    // Left: played on + RNG types
    auto *left = new QVBoxLayout;
    auto *hwBox = new QGroupBox(tr("Played on"));
    auto *hwLayout = new QVBoxLayout(hwBox);
    hardware_ = new QComboBox;
    customFps_ = new QDoubleSpinBox;
    customFps_->setRange(1, 1000);
    customFps_->setDecimals(4);
    customFps_->setSuffix(tr(" fps"));
    customFps_->setKeyboardTracking(false);
    hwLayout->addWidget(hardware_);
    hwLayout->addWidget(customFps_);
    left->addWidget(hwBox);
    auto *typeBox = new QGroupBox(tr("RNG"));
    auto *typeLayout = new QVBoxLayout(typeBox);
    types_ = new QListWidget;
    typeLayout->addWidget(types_);
    left->addWidget(typeBox, 1);
    auto *leftWidget = new QWidget;
    leftWidget->setLayout(left);
    left->setContentsMargins(0, 0, 0, 0);
    leftWidget->setFixedWidth(230);
    main->addWidget(leftWidget);

    // Middle: the type's fields
    auto *middle = new QVBoxLayout;
    title_ = new QLabel;
    QFont f = title_->font();
    f.setPointSizeF(f.pointSizeF() * 1.3);
    f.setBold(true);
    title_->setFont(f);
    middle->addWidget(title_);
    stack_ = new QStackedWidget;
    middle->addWidget(stack_, 1);
    note_ = new QLabel;
    note_->setWordWrap(true);
    note_->setStyleSheet(QStringLiteral("color: palette(placeholder-text);"));
    middle->addWidget(note_);
    auto *saveRow = new QHBoxLayout;
    saveButton_ = new QPushButton(tr("Save to profile"));
    saveButton_->setToolTip(tr("Keep this calibration: it pre-fills the field next time you pick this RNG."));
    saveState_ = new QLabel;
    saveRow->addWidget(saveButton_);
    saveRow->addWidget(saveState_, 1);
    middle->addLayout(saveRow);
    main->addLayout(middle, 3);

    // Right: the timer
    auto *right = new QVBoxLayout;
    auto *timerBox = new QGroupBox(tr("Timer"));
    auto *timerLayout = new QVBoxLayout(timerBox);
    display_ = new TimerDisplay;
    timerLayout->addWidget(display_);
    right->addWidget(timerBox, 1);
    auto *buttons = new QHBoxLayout;
    updateButton_ = new QPushButton(tr("Update"));
    updateButton_->setToolTip(tr("Enter what you hit in the \"Hit\" fields, then Update: the calibration is corrected."));
    startButton_ = new QPushButton(tr("Start"));
    startButton_->setDefault(true);
    startButton_->setMinimumWidth(130);
    startButton_->setToolTip(tr("Space"));
    buttons->addWidget(updateButton_);
    buttons->addWidget(startButton_, 1);
    right->addLayout(buttons);
    main->addLayout(right, 3);

    outputLabel_ = new QLabel;
    statusBar()->addPermanentWidget(outputLabel_);

    connect(consoles_, &QTabBar::currentChanged, this, &MainWindow::selectConsole);
    connect(types_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0)
        {
            selectType(types_->item(row)->data(Qt::UserRole).toString().toStdString());
        }
    });
    connect(hardware_, &QComboBox::currentIndexChanged, this, &MainWindow::hardwareChanged);
    connect(customFps_, &QDoubleSpinBox::valueChanged, this, &MainWindow::hardwareChanged);
    connect(saveButton_, &QPushButton::clicked, this, &MainWindow::saveCalibration);
    connect(updateButton_, &QPushButton::clicked, this, &MainWindow::calibrate);
    connect(startButton_, &QPushButton::clicked, this, &MainWindow::startStop);
    auto *space = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(space, &QShortcut::activated, this, &MainWindow::spacePressed);
    auto *ctrlReturn = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(ctrlReturn, &QShortcut::activated, this, &MainWindow::startStop);

    setCentralWidget(central);
    resize(1080, 560);
}

const Console &MainWindow::console() const
{
    return RngTypes::consoles()[size_t(consoleIndex_)];
}

const RngType *MainWindow::type() const
{
    return RngTypes::find(typeId_);
}

TimerPanel *MainWindow::panel() const
{
    auto it = panels_.find(typeId_);
    return it == panels_.end() ? nullptr : it->second;
}

void MainWindow::selectConsole(int index)
{
    if (index < 0)
    {
        return;
    }
    consoleIndex_ = index;
    const Console &c = console();

    hardware_->blockSignals(true);
    hardware_->clear();
    for (const auto &id : c.frameRates)
    {
        const FrameRate *f = FrameRates::find(id);
        QString label = QStringLiteral("%1 (%2)").arg(QString::fromStdString(f->name), QLocale::c().toString(f->fps, 'f', 4));
        if (!f->verified)
        {
            label += tr(", unverified");
        }
        hardware_->addItem(label, QString::fromStdString(id));
        hardware_->setItemData(hardware_->count() - 1, QString::fromStdString(f->source), Qt::ToolTipRole);
    }
    hardware_->addItem(tr("Custom frame rate"), QString::fromLatin1(FrameRates::Custom.data(), qsizetype(FrameRates::Custom.size())));
    auto hw = store_.hardware.find(c.id);
    int hi = hw == store_.hardware.end() ? 0 : hardware_->findData(QString::fromStdString(hw->second));
    hardware_->setCurrentIndex(std::max(0, hi));
    hardware_->blockSignals(false);
    customFps_->blockSignals(true);
    auto fps = store_.customFps.find(c.id);
    customFps_->setValue(fps == store_.customFps.end() ? 60.0 : fps->second);
    customFps_->blockSignals(false);
    customFps_->setVisible(hardware_->currentData().toString() == QLatin1String(FrameRates::Custom.data()));

    types_->blockSignals(true);
    types_->clear();
    for (const RngType *t : RngTypes::forConsole(c.id))
    {
        auto *item = new QListWidgetItem(QString::fromStdString(t->name));
        item->setData(Qt::UserRole, QString::fromStdString(t->id));
        types_->addItem(item);
    }
    types_->blockSignals(false);

    QSettings s;
    s.setValue("console", index);
    const QString last = s.value(QStringLiteral("type/%1").arg(QString::fromStdString(c.id))).toString();
    int row = 0;
    for (int i = 0; i < types_->count(); i++)
    {
        if (types_->item(i)->data(Qt::UserRole).toString() == last)
        {
            row = i;
        }
    }
    types_->setCurrentRow(row);
    selectType(types_->item(row)->data(Qt::UserRole).toString().toStdString());
}

TimerPanel *MainWindow::panelFor(const RngType &type)
{
    auto it = panels_.find(type.id);
    if (it != panels_.end())
    {
        return it->second;
    }
    TimerPanel *p = createPanel(type);
    panels_[type.id] = p;
    stack_->addWidget(p);
    prefill(type, p);
    connect(p, &TimerPanel::changed, this, [this] {
        updatePreview();
        updateSaveState();
    });
    connect(p, &TimerPanel::calibrationEdited, this, &MainWindow::updateSaveState);
    connect(p, &TimerPanel::sectionChanged, this, [this] {
        updateNote();
        updateSaveState();
    });
    connect(p, &TimerPanel::setTargetRequested, this, &MainWindow::setTarget);
    if (auto *xy = qobject_cast<XYTidPanel *>(p))
    {
        xy->setCountdown(settings_.beepCount, settings_.beepIntervalMs);
        connect(xy, &XYTidPanel::beepsChanged, controller_, &TimerController::setExternalBeeps);
        connect(xy, &XYTidPanel::message, this, [this](const QString &text) { statusBar()->showMessage(text, 10000); });
        connect(xy, &XYTidPanel::countdown, this, [this](double remaining, const QString &label) {
            display_->showLabelled(label, remaining, std::numeric_limits<double>::quiet_NaN());
        });
    }
    if (auto *custom = qobject_cast<CustomPanel *>(p))
    {
        connect(custom, &TimerPanel::addTargetRequested, this, [this, custom](double value, bool frames) {
            if (!controller_->isContinuous())
            {
                return;
            }
            if (!controller_->addTarget(custom->resolveTarget(calibrator(), value, frames)))
            {
                statusBar()->showMessage(tr("That target has already passed."), 4000);
            }
            custom->showTargets(controller_->targets(), calibrator());
        });
        connect(custom, &TimerPanel::nudgeRequested, this, [this, custom](int frames) {
            controller_->nudgeLastTarget(frames * calibrator().msPerFrame());
            custom->showTargets(controller_->targets(), calibrator());
        });
        connect(custom, &TimerPanel::removeLastRequested, this, [this, custom] {
            controller_->removeLastTarget();
            custom->showTargets(controller_->targets(), calibrator());
        });
    }
    return p;
}

void MainWindow::prefill(const RngType &type, TimerPanel *panel)
{
    for (int i = 0; i < panel->sectionCount(); i++)
    {
        const std::string key = RngTypes::calibrationKey(type, type.sections[size_t(i)]);
        auto it = store_.calibrations.find(key);
        panel->section(i)->setCalibration(it == store_.calibrations.end() ? Fields() : it->second);
        if (auto *custom = qobject_cast<CustomPanel *>(panel->section(i)))
        {
            auto phases = store_.customPhases.find(key);
            if (phases != store_.customPhases.end())
            {
                custom->setLayout(phases->second);
            }
            else
            {
                custom->setLayout({ { CustomUnit::Milliseconds, 5000, 0 }, { CustomUnit::Advances, 0, 0 } });
            }
        }
    }
}

void MainWindow::selectType(const std::string &id)
{
    const RngType *t = RngTypes::find(id);
    if (!t || controller_->isRunning())
    {
        return;
    }
    typeId_ = id;
    QSettings().setValue(QStringLiteral("type/%1").arg(QString::fromStdString(t->console)), QString::fromStdString(id));
    stack_->setCurrentWidget(panelFor(*t));
    title_->setText(QString::fromStdString(t->name));
    updateNote();
    updateSaveState();
    updatePreview();
}

void MainWindow::hardwareChanged()
{
    const QString id = hardware_->currentData().toString();
    customFps_->setVisible(id == QLatin1String(FrameRates::Custom.data()));
    store_.hardware[console().id] = id.toStdString();
    store_.customFps[console().id] = customFps_->value();
    saveStore();
    updatePreview();
    outputLabel_->setToolTip(QString());
    outputChanged();
}

Calibrator MainWindow::calibrator() const
{
    const std::string id = hardware_->currentData().toString().toStdString();
    return Calibrator(FrameRates::fps(id, customFps_->value()), settings_.precisionCalibration, settings_.minimumLengthSeconds);
}

std::string MainWindow::currentKey() const
{
    const RngType *t = type();
    TimerPanel *p = panel();
    if (!t || !p)
    {
        return {};
    }
    return RngTypes::calibrationKey(*t, t->sections[size_t(p->currentSection())]);
}

void MainWindow::updateNote()
{
    const RngType *t = type();
    TimerPanel *p = panel();
    const std::string note = t && p ? t->sections[size_t(p->currentSection())].note : std::string();
    note_->setText(QString::fromStdString(note));
    note_->setVisible(!note.empty());
}

void MainWindow::updateSaveState()
{
    TimerPanel *p = panel();
    if (!p)
    {
        return;
    }
    const std::string key = currentKey();
    TimerPanel *section = p->current();
    auto it = store_.calibrations.find(key);
    bool same = it != store_.calibrations.end() && it->second == section->calibration();
    if (auto *custom = qobject_cast<CustomPanel *>(section))
    {
        auto phases = store_.customPhases.find(key);
        auto mine = custom->layout();
        bool samePhases = phases != store_.customPhases.end() && phases->second.size() == mine.size();
        for (size_t i = 0; samePhases && i < mine.size(); i++)
        {
            samePhases = phases->second[i].unit == mine[i].unit && phases->second[i].target == mine[i].target
                && phases->second[i].calibration == mine[i].calibration;
        }
        same = same && samePhases;
    }
    if (it == store_.calibrations.end())
    {
        saveState_->setText(tr("No calibration saved for this RNG yet (defaults shown)."));
    }
    else if (same)
    {
        saveState_->setText(tr("Saved calibration."));
    }
    else
    {
        saveState_->setText(tr("<span style='color:#d08000'>Changed since saved.</span>"));
    }
}

void MainWindow::saveCalibration()
{
    TimerPanel *p = panel();
    if (!p)
    {
        return;
    }
    const std::string key = currentKey();
    store_.set(key, p->current()->calibration());
    if (auto *custom = qobject_cast<CustomPanel *>(p->current()))
    {
        store_.customPhases[key] = custom->layout();
    }
    saveStore();
    updateSaveState();
    QStringList parts;
    for (const auto &[name, value] : store_.calibrations[key])
    {
        if (name != "isContinuous")
        {
            parts << QStringLiteral("%1 %2").arg(QString::fromStdString(name), number(value));
        }
    }
    statusBar()->showMessage(tr("Saved: %1").arg(parts.join(QStringLiteral(", "))), 5000);
}

void MainWindow::saveStore()
{
    std::string error;
    if (!CalibrationFile::save(storePath().toStdString(), store_, &error))
    {
        statusBar()->showMessage(tr("Could not save: %1").arg(QString::fromStdString(error)), 10000);
    }
}

void MainWindow::updatePreview()
{
    TimerPanel *p = panel();
    if (!p || controller_->isRunning())
    {
        return;
    }
    const Calibrator cal = calibrator();
    if (p->current()->isContinuous())
    {
        display_->showPhases(0, std::numeric_limits<double>::quiet_NaN());
        display_->setMinutesBefore(-1);
        return;
    }
    const auto phases = p->current()->phases(cal);
    display_->showPhases(phases.empty() ? 0 : phases[0], phases.size() > 1 ? phases[1] : std::numeric_limits<double>::quiet_NaN());
    display_->setMinutesBefore(p->current()->minutesBefore(cal));
}

XYTidPanel *MainWindow::xyPanel() const
{
    TimerPanel *p = panel();
    return p ? qobject_cast<XYTidPanel *>(p->current()) : nullptr;
}

void MainWindow::spacePressed()
{
    XYTidPanel *xy = xyPanel();
    if (xy && xy->isSessionRunning())
    {
        xy->stepPressed(Clock::nowMs());
        return;
    }
    startStop();
}

void MainWindow::setRunningUi(bool running)
{
    startButton_->setText(running ? tr("Stop") : tr("Start"));
    consoles_->setEnabled(!running);
    types_->setEnabled(!running);
    hardware_->setEnabled(!running);
    customFps_->setEnabled(!running);
    updateButton_->setEnabled(!running);
    if (TimerPanel *p = panel())
    {
        p->setRunning(running);
    }
}

void MainWindow::startStop()
{
    TimerPanel *p = panel();
    if (!p)
    {
        return;
    }
    if (controller_->isRunning())
    {
        controller_->stop();
        if (XYTidPanel *xy = xyPanel())
        {
            xy->stopSession();
        }
        finished();
        return;
    }
    if (XYTidPanel *xy = xyPanel())
    {
        if (!xy->hasHandoff())
        {
            statusBar()->showMessage(tr("Fill in the state and the first tap."), 6000);
            return;
        }
        controller_->startExternal();
        xy->startSession(Clock::nowMs());
        setRunningUi(true);
        // Space must reach the step, not a text field that had the focus
        if (QWidget *focus = QApplication::focusWidget())
        {
            focus->clearFocus();
        }
        return;
    }
    if (p->current()->isContinuous())
    {
        controller_->startContinuous();
    }
    else
    {
        const auto phases = p->current()->phases(calibrator());
        if (phases.empty())
        {
            statusBar()->showMessage(tr("Enter the targets first."), 4000);
            return;
        }
        controller_->start(phases);
    }
    setRunningUi(true);
}

void MainWindow::finished()
{
    setRunningUi(false);
    if (controller_->slipCount() > 0)
    {
        statusBar()->showMessage(
            tr("The audio output stalled %n time(s); the beeps were put back on time.", nullptr, controller_->slipCount()), 8000);
    }
    QTimer::singleShot(800, this, &MainWindow::updatePreview);
}

void MainWindow::calibrate()
{
    TimerPanel *p = panel();
    if (p && p->current()->calibrate(calibrator()))
    {
        statusBar()->showMessage(tr("Calibration updated. \"Save to profile\" keeps it for next time."), 5000);
    }
    else
    {
        statusBar()->showMessage(tr("Enter what you hit first."), 4000);
    }
}

void MainWindow::setTarget()
{
    TimerPanel *p = panel();
    auto *gen3 = p ? qobject_cast<Gen3FramesPanel *>(p->current()) : nullptr;
    if (!gen3 || !controller_->isRunning())
    {
        return;
    }
    if (!controller_->setPhase(1, gen3->targetPhase(calibrator())))
    {
        statusBar()->showMessage(tr("That target has already passed."), 5000);
    }
}

void MainWindow::applySettings()
{
    controller_->setSound(loadSound(settings_.sound));
    controller_->setBeeps(settings_.beepCount, settings_.beepIntervalMs);
    for (const auto &[id, p] : panels_)
    {
        if (auto *xy = qobject_cast<XYTidPanel *>(p))
        {
            xy->setCountdown(settings_.beepCount, settings_.beepIntervalMs);
        }
    }
    controller_->setAudio(settings_.audio);
    controller_->setVisual(settings_.visual);
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(settings_, controller_, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        const AppSettings next = dialog.settings();
        const bool outputChangedByUser = next.output != settings_.output;
        settings_ = next;
        settings_.save();
        applyTheme(settings_.theme);
        if (outputChangedByUser)
        {
            QString error;
            if (!controller_->openOutput(settings_.output, &error) && !error.isEmpty())
            {
                statusBar()->showMessage(tr("Audio: %1").arg(error), 10000);
            }
            outputChanged();
        }
    }
    applySettings(); // also restores the beeps after a Test
    updatePreview();
}

void MainWindow::outputChanged()
{
    const QString name = controller_->outputName();
    outputLabel_->setText(tr("Output: %1  ·  %2 fps")
                              .arg(name.isEmpty() ? tr("none") : name,
                                   QLocale::c().toString(calibrator().msPerFrame() > 0 ? 1000.0 / calibrator().msPerFrame() : 0, 'f', 4)));
    outputLabel_->setToolTip(controller_->outputDetails());
}

void MainWindow::listen()
{
    server_ = new QLocalServer(this);
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_->listen(QString::fromLatin1(XYTid::SOCKET_NAME)))
    {
        // A crashed instance can leave the socket file behind (Unix); a running one keeps it
        QLocalSocket probe;
        probe.connectToServer(QString::fromLatin1(XYTid::SOCKET_NAME));
        if (!probe.waitForConnected(300))
        {
            QLocalServer::removeServer(QString::fromLatin1(XYTid::SOCKET_NAME));
            server_->listen(QString::fromLatin1(XYTid::SOCKET_NAME));
        }
    }
    connect(server_, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = server_->nextPendingConnection())
        {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                while (socket->canReadLine())
                {
                    socket->write(receiveHandoff(socket->readLine().trimmed()) + "\n");
                    socket->flush();
                }
            });
        }
    });
}

QByteArray MainWindow::receiveHandoff(const QByteArray &line)
{
    auto handoff = XYTid::Handoff::fromString(line.toStdString());
    if (!handoff)
    {
        return "ERROR";
    }
    if (controller_->isRunning())
    {
        return "BUSY";
    }
    const auto &list = RngTypes::consoles();
    for (int i = 0; i < int(list.size()); i++)
    {
        if (list[size_t(i)].id == "3ds")
        {
            consoles_->setCurrentIndex(i); // selects the console's last type
        }
    }
    for (int row = 0; row < types_->count(); row++)
    {
        if (types_->item(row)->data(Qt::UserRole).toString() == QLatin1String("xy-tid"))
        {
            types_->setCurrentRow(row);
        }
    }
    XYTidPanel *xy = xyPanel();
    if (!xy || !xy->load(*handoff))
    {
        return "ERROR";
    }
    startStop(); // the blink beeps start at once: you are on the professor's screen
    raise();
    activateWindow();
    return "OK";
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    controller_->stop();
    QSettings s;
    s.setValue("geometry", saveGeometry());
    event->accept();
}
