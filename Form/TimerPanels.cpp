/*
 * This file is part of PokeTimerPlus
 */

#include "TimerPanels.hpp"
#include "XYTidPanel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <limits>

// ---------------------------------------------------------------- base

QSpinBox *TimerPanel::spin(int min, int max, const QString &suffix)
{
    auto *s = new QSpinBox;
    s->setRange(min, max);
    s->setSuffix(suffix);
    s->setKeyboardTracking(false);
    s->setAccelerated(true);
    return s;
}

QSpinBox *TimerPanel::targetSpin(int max, const QString &suffix)
{
    QSpinBox *s = spin(0, max, suffix);
    s->setSpecialValueText(QStringLiteral(" ")); // 0 shows empty: targets are typed for each attempt
    return s;
}

QDoubleSpinBox *TimerPanel::doubleSpin(const QString &suffix)
{
    auto *s = new QDoubleSpinBox;
    s->setRange(-1e7, 1e7);
    s->setDecimals(2);
    s->setSuffix(suffix);
    s->setKeyboardTracking(false);
    return s;
}

QLineEdit *TimerPanel::hitEdit(const QString &placeholder)
{
    auto *e = new QLineEdit;
    auto *v = new QDoubleValidator(-1e8, 1e8, 3, e);
    v->setLocale(QLocale::c());
    v->setNotation(QDoubleValidator::StandardNotation);
    e->setValidator(v);
    e->setPlaceholderText(placeholder);
    e->setClearButtonEnabled(true);
    return e;
}

std::optional<double> TimerPanel::hit(const QLineEdit *edit)
{
    bool ok = false;
    double v = QLocale::c().toDouble(edit->text().trimmed(), &ok);
    return ok ? std::optional<double>(v) : std::nullopt;
}

double TimerPanel::field(const Fields &f, const char *key, double fallback)
{
    auto it = f.find(key);
    return it == f.end() ? fallback : it->second;
}

void TimerPanel::watch(QSpinBox *s, bool isCalibration)
{
    connect(s, &QSpinBox::valueChanged, this, [this, isCalibration] {
        if (!loading_)
        {
            if (isCalibration)
            {
                emit calibrationEdited();
            }
            emit changed();
        }
    });
}

void TimerPanel::watch(QDoubleSpinBox *s, bool isCalibration)
{
    connect(s, &QDoubleSpinBox::valueChanged, this, [this, isCalibration] {
        if (!loading_)
        {
            if (isCalibration)
            {
                emit calibrationEdited();
            }
            emit changed();
        }
    });
}

namespace
{
    QGroupBox *group(const QString &title, QFormLayout *&form)
    {
        auto *box = new QGroupBox(title);
        form = new QFormLayout(box);
        return box;
    }

    int toInt(double v)
    {
        return static_cast<int>(std::lround(v));
    }
}

// ---------------------------------------------------------------- Gen 3 frames

Gen3FramesPanel::Gen3FramesPanel(QWidget *parent) : TimerPanel(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    QFormLayout *targets, *calibration;
    layout->addWidget(group(tr("Targets"), targets));
    layout->addWidget(group(tr("Calibration"), calibration));
    layout->addStretch();

    preTimer_ = spin(0, 3600000, tr(" ms"));
    preTimer_->setValue(5000);
    targetFrame_ = targetSpin(100000000);
    variable_ = new QCheckBox(tr("Variable target (enter the frame while the timer runs)"));
    setTarget_ = new QPushButton(tr("Set"));
    auto *row = new QHBoxLayout;
    row->addWidget(targetFrame_, 1);
    row->addWidget(setTarget_);
    targets->addRow(tr("Pre-timer"), preTimer_);
    targets->addRow(tr("Target frame"), row);
    targets->addRow(QString(), variable_);

    calibration_ = doubleSpin(tr(" ms"));
    frameHit_ = hitEdit(tr("frame you hit"));
    calibration->addRow(tr("Calibration"), calibration_);
    calibration->addRow(tr("Frame hit"), frameHit_);

    watch(preTimer_, false);
    watch(targetFrame_, false);
    watch(calibration_, true);
    connect(variable_, &QCheckBox::toggled, this, [this] {
        updateRows();
        emit changed();
    });
    connect(setTarget_, &QPushButton::clicked, this, &TimerPanel::setTargetRequested);
    connect(targetFrame_, &QSpinBox::editingFinished, this, [this] {
        if (running_ && variable_->isChecked())
        {
            emit setTargetRequested();
        }
    });
    updateRows();
}

void Gen3FramesPanel::updateRows()
{
    setTarget_->setVisible(variable_->isChecked());
    setTarget_->setEnabled(running_);
    variable_->setEnabled(!running_);
    preTimer_->setEnabled(!running_);
    targetFrame_->setEnabled(!running_ || variable_->isChecked());
}

void Gen3FramesPanel::setRunning(bool running)
{
    running_ = running;
    updateRows();
    if (running && variable_->isChecked())
    {
        targetFrame_->setFocus();
        targetFrame_->selectAll();
    }
}

Fields Gen3FramesPanel::calibration() const
{
    return { { "ms", calibration_->value() } };
}

void Gen3FramesPanel::setCalibration(const Fields &f)
{
    loading_ = true;
    calibration_->setValue(field(f, "ms", 0));
    loading_ = false;
    emit changed();
}

std::vector<double> Gen3FramesPanel::phases(const Calibrator &calibrator) const
{
    Gen3Targets t { variable_->isChecked() ? Gen3Mode::VariableTarget : Gen3Mode::Standard, preTimer_->value(), targetFrame_->value() };
    if (t.mode == Gen3Mode::Standard && t.targetFrame == 0)
    {
        return {};
    }
    return Timers::gen3Phases(calibrator, t, calibration_->value());
}

double Gen3FramesPanel::targetPhase(const Calibrator &calibrator) const
{
    return Timers::framePhase(calibrator, targetFrame_->value(), calibration_->value());
}

bool Gen3FramesPanel::calibrate(const Calibrator &calibrator)
{
    auto h = hit(frameHit_);
    if (!h || targetFrame_->value() == 0)
    {
        return false;
    }
    calibration_->setValue(calibration_->value() + Timers::frameCalibration(calibrator, targetFrame_->value(), toInt(*h)));
    frameHit_->clear();
    return true;
}

// ---------------------------------------------------------------- FRLG seed + advances

SeedAdvancesPanel::SeedAdvancesPanel(QWidget *parent) : TimerPanel(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    QFormLayout *seedForm, *advForm;
    layout->addWidget(group(tr("Initial seed"), seedForm));
    layout->addWidget(group(tr("Advances"), advForm));
    layout->addStretch();

    seed_ = targetSpin(3600000, tr(" ms"));
    seed_->setToolTip(tr("Seed time from Ten Lines / PokeFinderPlus (ms)."));
    seedCalibration_ = doubleSpin(tr(" ms"));
    seedHit_ = hitEdit(tr("seed time you hit (ms)"));
    seedForm->addRow(tr("Seed time"), seed_);
    seedForm->addRow(tr("Calibration"), seedCalibration_);
    seedForm->addRow(tr("Hit"), seedHit_);

    advances_ = targetSpin(100000000);
    advancesCalibration_ = spin(-100000000, 100000000);
    advanceHit_ = hitEdit(tr("advance you hit"));
    advForm->addRow(tr("Target advance"), advances_);
    advForm->addRow(tr("Calibration (advances)"), advancesCalibration_);
    advForm->addRow(tr("Hit"), advanceHit_);

    watch(seed_, false);
    watch(advances_, false);
    watch(seedCalibration_, true);
    watch(advancesCalibration_, true);
}

Fields SeedAdvancesPanel::calibration() const
{
    return { { "seedMs", seedCalibration_->value() }, { "advances", double(advancesCalibration_->value()) } };
}

void SeedAdvancesPanel::setCalibration(const Fields &f)
{
    loading_ = true;
    seedCalibration_->setValue(field(f, "seedMs", 0));
    advancesCalibration_->setValue(toInt(field(f, "advances", 0)));
    loading_ = false;
    emit changed();
}

std::vector<double> SeedAdvancesPanel::phases(const Calibrator &calibrator) const
{
    if (seed_->value() == 0 || advances_->value() == 0)
    {
        return {};
    }
    // Phase 1: power on -> seed (press at its end); phase 2: the advances, counted in frames.
    return { seed_->value() + seedCalibration_->value(), calibrator.toMilliseconds(advances_->value() + advancesCalibration_->value()) };
}

bool SeedAdvancesPanel::calibrate(const Calibrator &)
{
    bool any = false;
    if (auto h = hit(seedHit_); h && seed_->value() != 0)
    {
        seedCalibration_->setValue(seedCalibration_->value() + (seed_->value() - *h));
        seedHit_->clear();
        any = true;
    }
    if (auto h = hit(advanceHit_); h && advances_->value() != 0)
    {
        advancesCalibration_->setValue(advancesCalibration_->value() + (advances_->value() - toInt(*h)));
        advanceHit_->clear();
        any = true;
    }
    return any;
}

// ---------------------------------------------------------------- Gen 4

Gen4DelayPanel::Gen4DelayPanel(QWidget *parent) : TimerPanel(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    QFormLayout *targets, *calibration;
    layout->addWidget(group(tr("Targets"), targets));
    layout->addWidget(group(tr("Calibration"), calibration));
    layout->addStretch();

    targetDelay_ = targetSpin(10000000);
    targetSecond_ = spin(0, 59);
    targets->addRow(tr("Target delay"), targetDelay_);
    targets->addRow(tr("Target second"), targetSecond_);

    calibratedDelay_ = spin(-10000000, 10000000);
    calibratedSecond_ = spin(0, 59);
    delayHit_ = hitEdit(tr("delay you hit"));
    calibration->addRow(tr("Calibrated delay"), calibratedDelay_);
    calibration->addRow(tr("Calibrated second"), calibratedSecond_);
    calibration->addRow(tr("Delay hit"), delayHit_);

    watch(targetDelay_, false);
    watch(targetSecond_, false);
    watch(calibratedDelay_, true);
    watch(calibratedSecond_, true);
    setCalibration({});
}

Gen4Targets Gen4DelayPanel::targets() const
{
    return { targetDelay_->value(), targetSecond_->value() };
}

Fields Gen4DelayPanel::calibration() const
{
    return { { "delay", double(calibratedDelay_->value()) }, { "second", double(calibratedSecond_->value()) } };
}

void Gen4DelayPanel::setCalibration(const Fields &f)
{
    loading_ = true;
    calibratedDelay_->setValue(toInt(field(f, "delay", 500))); // EonTimer defaults
    calibratedSecond_->setValue(toInt(field(f, "second", 14)));
    loading_ = false;
    emit changed();
}

std::vector<double> Gen4DelayPanel::phases(const Calibrator &calibrator) const
{
    if (targetDelay_->value() == 0)
    {
        return {};
    }
    return Timers::gen4Phases(calibrator, targets(), { calibratedDelay_->value(), calibratedSecond_->value() });
}

bool Gen4DelayPanel::calibrate(const Calibrator &calibrator)
{
    auto h = hit(delayHit_);
    if (!h || targetDelay_->value() == 0)
    {
        return false;
    }
    calibratedDelay_->setValue(calibratedDelay_->value() + Timers::gen4CalibratedDelayDelta(calibrator, targets(), toInt(*h)));
    delayHit_->clear();
    return true;
}

int Gen4DelayPanel::minutesBefore(const Calibrator &calibrator) const
{
    return targetDelay_->value() == 0 ? -1 : Timers::gen4MinutesBeforeTarget(calibrator, targets());
}

// ---------------------------------------------------------------- Gen 5

Gen5Panel::Gen5Panel(QWidget *parent) : TimerPanel(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(group(tr("Timer"), form_));
    layout->addStretch();

    mode_ = new QComboBox;
    for (Gen5Mode m : { Gen5Mode::Standard, Gen5Mode::CGear, Gen5Mode::Entralink, Gen5Mode::EntralinkPlus })
    {
        mode_->addItem(QString::fromUtf8(std::string(toString(m)).c_str()), int(m));
    }
    targetDelay_ = targetSpin(10000000);
    targetSecond_ = spin(0, 59);
    targetAdvances_ = targetSpin(10000000);
    calibration_ = spin(-10000000, 10000000);
    entralink_ = spin(-10000000, 10000000);
    frame_ = doubleSpin(tr(" ms"));
    delayHit_ = hitEdit(tr("delay you hit"));
    secondHit_ = hitEdit(tr("second you hit"));
    advancesHit_ = hitEdit(tr("advances you hit"));
    form_->addRow(tr("Mode"), mode_);
    form_->addRow(tr("Target delay"), targetDelay_);
    form_->addRow(tr("Target second"), targetSecond_);
    form_->addRow(tr("Target advances"), targetAdvances_);
    form_->addRow(tr("Calibration (all Gen 5)"), calibration_);
    form_->addRow(tr("Entralink calibration"), entralink_);
    form_->addRow(tr("Frame calibration"), frame_);
    form_->addRow(tr("Delay hit"), delayHit_);
    form_->addRow(tr("Second hit"), secondHit_);
    form_->addRow(tr("Advances hit"), advancesHit_);

    connect(mode_, &QComboBox::currentIndexChanged, this, [this] {
        updateRows();
        emit changed();
    });
    watch(targetDelay_, false);
    watch(targetSecond_, false);
    watch(targetAdvances_, false);
    watch(calibration_, true);
    watch(entralink_, true);
    watch(frame_, true);
    setCalibration({});
    updateRows();
}

void Gen5Panel::updateRows()
{
    const Gen5Mode m = Gen5Mode(mode_->currentData().toInt());
    const bool delay = m != Gen5Mode::Standard;
    const bool entralink = m == Gen5Mode::Entralink || m == Gen5Mode::EntralinkPlus;
    const bool plus = m == Gen5Mode::EntralinkPlus;
    form_->setRowVisible(targetDelay_, delay);
    form_->setRowVisible(targetAdvances_, plus);
    form_->setRowVisible(entralink_, entralink);
    form_->setRowVisible(frame_, plus);
    form_->setRowVisible(delayHit_, delay);
    form_->setRowVisible(secondHit_, m != Gen5Mode::CGear);
    form_->setRowVisible(advancesHit_, plus);
}

Gen5Targets Gen5Panel::targets() const
{
    return { Gen5Mode(mode_->currentData().toInt()), targetDelay_->value(), targetSecond_->value(), targetAdvances_->value() };
}

Gen5Calibration Gen5Panel::cal() const
{
    return { calibration_->value(), entralink_->value(), frame_->value() };
}

Fields Gen5Panel::calibration() const
{
    return { { "calibration", double(calibration_->value()) }, { "entralink", double(entralink_->value()) }, { "frame", frame_->value() } };
}

void Gen5Panel::setCalibration(const Fields &f)
{
    loading_ = true;
    calibration_->setValue(toInt(field(f, "calibration", -95))); // EonTimer defaults
    entralink_->setValue(toInt(field(f, "entralink", 256)));
    frame_->setValue(field(f, "frame", 0));
    loading_ = false;
    emit changed();
}

std::vector<double> Gen5Panel::phases(const Calibrator &calibrator) const
{
    const Gen5Targets t = targets();
    if (t.mode != Gen5Mode::Standard && t.targetDelay == 0)
    {
        return {};
    }
    return Timers::gen5Phases(calibrator, t, cal());
}

bool Gen5Panel::calibrate(const Calibrator &calibrator)
{
    auto asInt = [](std::optional<double> v) { return v ? std::optional<int>(toInt(*v)) : std::nullopt; };
    Gen5Hits hits { asInt(hit(delayHit_)), asInt(hit(secondHit_)), asInt(hit(advancesHit_)) };
    if (!hits.delay && !hits.second && !hits.advances)
    {
        return false;
    }
    Gen5Calibration after = Timers::gen5Calibrate(calibrator, targets(), cal(), hits);
    calibration_->setValue(after.calibration);
    entralink_->setValue(after.entralink);
    frame_->setValue(after.frame);
    delayHit_->clear();
    secondHit_->clear();
    advancesHit_->clear();
    return true;
}

int Gen5Panel::minutesBefore(const Calibrator &calibrator) const
{
    return Timers::gen5MinutesBeforeTarget(calibrator, targets());
}

// ---------------------------------------------------------------- Custom

CustomPanel::CustomPanel(QWidget *parent) : TimerPanel(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    continuous_ = new QCheckBox(tr("Continuous: the timer keeps running, add targets while it runs"));
    layout->addWidget(continuous_);
    stack_ = new QStackedWidget;
    layout->addWidget(stack_, 1);

    // Phases
    auto *phasesBox = new QGroupBox(tr("Phases"));
    auto *phasesLayout = new QVBoxLayout(phasesBox);
    table_ = new QTableWidget(0, 4);
    table_->setHorizontalHeaderLabels({ tr("Unit"), tr("Target"), tr("Calibration (ms)"), tr("Hit") });
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    phasesLayout->addWidget(table_);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add phase"));
    auto *remove = new QPushButton(tr("Remove"));
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    phasesLayout->addLayout(buttons);
    stack_->addWidget(phasesBox);
    connect(add, &QPushButton::clicked, this, [this] {
        addRow({});
        emit changed();
        emit calibrationEdited();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        int r = table_->currentRow() >= 0 ? table_->currentRow() : table_->rowCount() - 1;
        if (r >= 0)
        {
            table_->removeRow(r);
            emit changed();
            emit calibrationEdited();
        }
    });

    // Continuous
    auto *contBox = new QGroupBox(tr("Targets"));
    auto *contLayout = new QVBoxLayout(contBox);
    auto *form = new QFormLayout;
    target_ = targetSpin(std::numeric_limits<int>::max());
    unit_ = new QComboBox;
    unit_->addItem(tr("frames"), true);
    unit_->addItem(tr("ms"), false);
    add_ = new QPushButton(tr("Add target"));
    add_->setToolTip(tr("Or press Enter in the target field"));
    auto *targetRow = new QHBoxLayout;
    targetRow->addWidget(target_, 1);
    targetRow->addWidget(unit_);
    targetRow->addWidget(add_);
    continuousCalibration_ = doubleSpin(tr(" ms"));
    continuousHit_ = hitEdit(tr("what you hit on the last target (same unit)"));
    form->addRow(tr("Target"), targetRow);
    form->addRow(tr("Calibration"), continuousCalibration_);
    form->addRow(tr("Hit"), continuousHit_);
    contLayout->addLayout(form);
    targets_ = new QListWidget;
    contLayout->addWidget(targets_, 1);
    auto *nudge = new QHBoxLayout;
    minus_ = new QPushButton(tr("−1 frame"));
    plus_ = new QPushButton(tr("+1 frame"));
    removeLast_ = new QPushButton(tr("Remove last"));
    nudge->addWidget(minus_);
    nudge->addWidget(plus_);
    nudge->addWidget(removeLast_);
    nudge->addStretch();
    contLayout->addLayout(nudge);
    stack_->addWidget(contBox);

    auto requestAdd = [this] {
        if (running_ && target_->value() > 0)
        {
            typed_.push_back({ double(target_->value()), unit_->currentData().toBool() });
            emit addTargetRequested(target_->value(), unit_->currentData().toBool());
            target_->selectAll();
        }
    };
    connect(add_, &QPushButton::clicked, this, requestAdd);
    connect(target_, &QSpinBox::editingFinished, this, [this, requestAdd] {
        if (target_->hasFocus())
        {
            requestAdd();
        }
    });
    connect(minus_, &QPushButton::clicked, this, [this] { emit nudgeRequested(-1); });
    connect(plus_, &QPushButton::clicked, this, [this] { emit nudgeRequested(1); });
    connect(removeLast_, &QPushButton::clicked, this, [this] {
        if (!typed_.empty())
        {
            typed_.pop_back();
        }
        emit removeLastRequested();
    });
    watch(continuousCalibration_, true);
    connect(continuous_, &QCheckBox::toggled, this, [this] {
        updateMode();
        emit changed();
        emit calibrationEdited();
    });
    updateMode();
}

void CustomPanel::updateMode()
{
    stack_->setCurrentIndex(continuous_->isChecked() ? 1 : 0);
    continuous_->setEnabled(!running_);
    const bool canEdit = running_ && continuous_->isChecked();
    add_->setEnabled(canEdit);
    minus_->setEnabled(canEdit);
    plus_->setEnabled(canEdit);
    removeLast_->setEnabled(canEdit);
    table_->setEnabled(!running_);
}

void CustomPanel::setRunning(bool running)
{
    running_ = running;
    if (running)
    {
        typed_.clear();
        targets_->clear();
    }
    updateMode();
    if (running && continuous_->isChecked())
    {
        target_->setFocus();
        target_->selectAll();
    }
}

bool CustomPanel::isContinuous() const
{
    return continuous_->isChecked();
}

void CustomPanel::addRow(const CustomPhase &phase)
{
    const int r = table_->rowCount();
    table_->insertRow(r);
    auto *unit = new QComboBox;
    for (CustomUnit u : { CustomUnit::Milliseconds, CustomUnit::Advances, CustomUnit::SeedHex })
    {
        unit->addItem(QString::fromUtf8(std::string(toString(u)).c_str()), int(u));
    }
    unit->setCurrentIndex(unit->findData(int(phase.unit)));
    auto *target = spin(0, std::numeric_limits<int>::max());
    target->setValue(phase.target);
    target->setDisplayIntegerBase(phase.unit == CustomUnit::SeedHex ? 16 : 10);
    auto *calibration = doubleSpin();
    calibration->setValue(phase.calibration);
    table_->setCellWidget(r, 0, unit);
    table_->setCellWidget(r, 1, target);
    table_->setCellWidget(r, 2, calibration);
    table_->setCellWidget(r, 3, hitEdit(tr("hit")));
    connect(unit, &QComboBox::currentIndexChanged, this, [this, unit, target] {
        target->setDisplayIntegerBase(CustomUnit(unit->currentData().toInt()) == CustomUnit::SeedHex ? 16 : 10);
        emit changed();
        emit calibrationEdited();
    });
    watch(target, false);
    watch(calibration, true);
}

std::vector<CustomPhase> CustomPanel::layout() const
{
    std::vector<CustomPhase> out;
    for (int r = 0; r < table_->rowCount(); r++)
    {
        auto *unit = qobject_cast<QComboBox *>(table_->cellWidget(r, 0));
        auto *target = qobject_cast<QSpinBox *>(table_->cellWidget(r, 1));
        auto *calibration = qobject_cast<QDoubleSpinBox *>(table_->cellWidget(r, 2));
        out.push_back({ CustomUnit(unit->currentData().toInt()), target->value(), calibration->value() });
    }
    return out;
}

void CustomPanel::setLayout(const std::vector<CustomPhase> &phases)
{
    loading_ = true;
    table_->setRowCount(0);
    for (const auto &p : phases)
    {
        addRow(p);
    }
    loading_ = false;
    emit changed();
}

Fields CustomPanel::calibration() const
{
    return { { "continuous", continuousCalibration_->value() }, { "isContinuous", continuous_->isChecked() ? 1.0 : 0.0 } };
}

void CustomPanel::setCalibration(const Fields &f)
{
    loading_ = true;
    continuousCalibration_->setValue(field(f, "continuous", 0));
    continuous_->setChecked(field(f, "isContinuous", 0) != 0);
    updateMode();
    loading_ = false;
    emit changed();
}

std::vector<double> CustomPanel::phases(const Calibrator &calibrator) const
{
    if (continuous_->isChecked())
    {
        return {};
    }
    return Timers::customPhases(calibrator, layout());
}

bool CustomPanel::calibrate(const Calibrator &calibrator)
{
    bool any = false;
    if (continuous_->isChecked())
    {
        auto h = hit(continuousHit_);
        if (h && !typed_.empty())
        {
            const auto [value, frames] = typed_.back();
            const double deltaMs = frames ? (value - *h) * calibrator.msPerFrame() : value - *h;
            continuousCalibration_->setValue(continuousCalibration_->value() + deltaMs);
            continuousHit_->clear();
            any = true;
        }
        return any;
    }
    for (int r = 0; r < table_->rowCount(); r++)
    {
        auto *unit = qobject_cast<QComboBox *>(table_->cellWidget(r, 0));
        auto *target = qobject_cast<QSpinBox *>(table_->cellWidget(r, 1));
        auto *calibration = qobject_cast<QDoubleSpinBox *>(table_->cellWidget(r, 2));
        auto *hitBox = qobject_cast<QLineEdit *>(table_->cellWidget(r, 3));
        auto h = hit(hitBox);
        if (!h)
        {
            continue;
        }
        // Same rule as EonTimer's frame calibration: move by the difference, in ms.
        const bool ms = CustomUnit(unit->currentData().toInt()) == CustomUnit::Milliseconds;
        const double delta = ms ? target->value() - *h : calibrator.toMilliseconds(target->value() - toInt(*h));
        calibration->setValue(calibration->value() + delta);
        hitBox->clear();
        any = true;
    }
    return any;
}

double CustomPanel::resolveTarget(const Calibrator &calibrator, double value, bool frames) const
{
    // FlowTimer: frame / fps, not rounded to whole ms.
    return (frames ? value * calibrator.msPerFrame() : value) + continuousCalibration_->value();
}

void CustomPanel::showTargets(const std::vector<double> &targetsMs, const Calibrator &calibrator)
{
    targets_->clear();
    for (size_t i = 0; i < targetsMs.size(); i++)
    {
        QString typed;
        if (i < typed_.size())
        {
            typed = typed_[i].second ? tr("%1 frames").arg(typed_[i].first, 0, 'f', 0) : tr("%1 ms").arg(typed_[i].first, 0, 'f', 0);
        }
        targets_->addItem(tr("%1  →  %2 s").arg(typed).arg(QLocale::c().toString(targetsMs[i] / 1000.0, 'f', 3)));
    }
    (void)calibrator;
}

// ---------------------------------------------------------------- sections

SectionedPanel::SectionedPanel(const std::vector<RngSection> &sections, TimerTemplate timer, QWidget *parent) : TimerPanel(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    bar_ = new QTabBar;
    bar_->setExpanding(false);
    stack_ = new QStackedWidget;
    layout->addWidget(bar_);
    layout->addWidget(stack_, 1);
    for (const auto &s : sections)
    {
        RngType single { "", "", "", timer, "", { s } };
        TimerPanel *p = createPanel(single);
        panels_.push_back(p);
        bar_->addTab(QString::fromStdString(s.name));
        stack_->addWidget(p);
        connect(p, &TimerPanel::changed, this, &TimerPanel::changed);
        connect(p, &TimerPanel::calibrationEdited, this, &TimerPanel::calibrationEdited);
        connect(p, &TimerPanel::setTargetRequested, this, &TimerPanel::setTargetRequested);
    }
    connect(bar_, &QTabBar::currentChanged, this, [this](int i) {
        stack_->setCurrentIndex(i);
        emit sectionChanged();
        emit changed();
    });
}

int SectionedPanel::sectionCount() const
{
    return static_cast<int>(panels_.size());
}

TimerPanel *SectionedPanel::section(int index)
{
    return panels_[size_t(index)];
}

TimerPanel *SectionedPanel::current()
{
    return panels_[size_t(bar_->currentIndex())];
}

int SectionedPanel::currentSection() const
{
    return bar_->currentIndex();
}

Fields SectionedPanel::calibration() const
{
    return panels_[size_t(bar_->currentIndex())]->calibration();
}

void SectionedPanel::setCalibration(const Fields &fields)
{
    panels_[size_t(bar_->currentIndex())]->setCalibration(fields);
}

std::vector<double> SectionedPanel::phases(const Calibrator &calibrator) const
{
    return panels_[size_t(bar_->currentIndex())]->phases(calibrator);
}

bool SectionedPanel::calibrate(const Calibrator &calibrator)
{
    return panels_[size_t(bar_->currentIndex())]->calibrate(calibrator);
}

void SectionedPanel::setRunning(bool running)
{
    bar_->setEnabled(!running);
    panels_[size_t(bar_->currentIndex())]->setRunning(running);
}

TimerPanel *createPanel(const RngType &type)
{
    if (type.sections.size() > 1)
    {
        return new SectionedPanel(type.sections, type.timer);
    }
    switch (type.timer)
    {
    case TimerTemplate::Gen3Frames:
        return new Gen3FramesPanel;
    case TimerTemplate::SeedAdvances:
        return new SeedAdvancesPanel;
    case TimerTemplate::Gen4Delay:
        return new Gen4DelayPanel;
    case TimerTemplate::Gen5:
        return new Gen5Panel;
    case TimerTemplate::Custom:
        return new CustomPanel;
    case TimerTemplate::XYTid:
        return new XYTidPanel;
    }
    return new CustomPanel;
}
