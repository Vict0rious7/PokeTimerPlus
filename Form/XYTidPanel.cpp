/*
 * This file is part of PokeTimerPlus
 * XY TID: the blink timeline and the wait after the 4th step (6genTidSearch's 消費数管理 tab + the FlowTimer countdown it
 * was used with). Method and values: BZL's 6genTidSearch (used with permission), https://blog.bzl-web.com/entry/2020/08/31/202306
 */

#include "XYTidPanel.hpp"

#include <Core/Util/Clock.hpp>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

namespace
{
    // Scenes at which Space is pressed together with A (6genTidSearch's ステップ labels)
    const char *const stepNames[] = {
        QT_TRANSLATE_NOOP("XYTidPanel", "1. Leave the professor's speech (A → gender screen)"),
        QT_TRANSLATE_NOOP("XYTidPanel", "2. Open the name input screen (A)"),
        QT_TRANSLATE_NOOP("XYTidPanel", "3. Name decided (OK)"),
        QT_TRANSLATE_NOOP("XYTidPanel", "4. Name confirmed (Yes)"),
    };

    constexpr XYTid::u32 LIST_ROWS = 3000; // about an hour of IDs
    constexpr double BEEP_HORIZON_MS = 45000;

    QString minutes(double ms)
    {
        if (!std::isfinite(ms))
        {
            return {};
        }
        const bool negative = ms < 0;
        const qint64 s = qint64(std::floor(std::abs(ms) / 1000));
        return QStringLiteral("%1%2:%3").arg(negative ? QStringLiteral("-") : QString()).arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
    }
}

XYTidPanel::XYTidPanel(QWidget *parent) : TimerPanel(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *from = new QGroupBox(tr("From PokeFinderPlus (Gen 6 → IDs)"));
    auto *fromLayout = new QHBoxLayout(from);
    source_ = new QLabel(tr("Nothing yet: in PokeFinderPlus, pick an ID and press \"Send to PokeTimerPlus\"."));
    source_->setObjectName("labelSource");
    source_->setWordWrap(true);
    paste_ = new QPushButton(tr("Paste"));
    paste_->setObjectName("pushButtonPaste");
    paste_->setToolTip(tr("Use \"Copy for PokeTimerPlus\" in PokeFinderPlus, then Paste here."));
    fromLayout->addWidget(source_, 1);
    fromLayout->addWidget(paste_);
    layout->addWidget(from);

    auto *run = new QGroupBox(tr("Opening"));
    auto *form = new QFormLayout(run);
    auto *gender = new QWidget;
    auto *genderRow = new QHBoxLayout(gender);
    genderRow->setContentsMargins(0, 0, 0, 0);
    boy_ = new QRadioButton(tr("Boy"));
    girl_ = new QRadioButton(tr("Girl"));
    boy_->setChecked(true);
    genderRow->addWidget(boy_);
    genderRow->addWidget(girl_);
    genderRow->addStretch();
    form->addRow(tr("Protagonist"), gender);

    auto *sync = new QWidget;
    auto *syncRow = new QHBoxLayout(sync);
    syncRow->setContentsMargins(0, 0, 0, 0);
    slower_ = new QPushButton(tr("Slower"));
    faster_ = new QPushButton(tr("Faster"));
    slower_->setObjectName("pushButtonSlower");
    faster_->setObjectName("pushButtonFaster");
    slower_->setToolTip(tr("The beep comes before the blink: one frame later"));
    faster_->setToolTip(tr("The blink comes before the beep: one frame earlier"));
    offsetLabel_ = new QLabel;
    syncRow->addWidget(slower_);
    syncRow->addWidget(faster_);
    syncRow->addWidget(offsetLabel_, 1);
    form->addRow(tr("Beep on the blink"), sync);

    auto *stepRow = new QWidget;
    auto *stepLayout = new QHBoxLayout(stepRow);
    stepLayout->setContentsMargins(0, 0, 0, 0);
    stepLabel_ = new QLabel;
    stepLabel_->setObjectName("labelStep");
    QFont bold = stepLabel_->font();
    bold.setBold(true);
    stepLabel_->setFont(bold);
    step_ = new QPushButton(tr("Step  (Space)"));
    step_->setObjectName("pushButtonStep");
    step_->setFocusPolicy(Qt::NoFocus);
    stepLayout->addWidget(stepLabel_, 1);
    stepLayout->addWidget(step_);
    form->addRow(tr("Next step"), stepRow);
    layout->addWidget(run);

    table_ = new QTableWidget(0, 8);
    table_->setObjectName("tableWidgetWait");
    table_->setHorizontalHeaderLabels(
        { tr("Advance"), tr("TID"), tr("SID"), tr("TSV"), tr("TRV"), tr("Wait (f)"), tr("Window (f)"), tr("A in") });
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->setToolTip(tr("After the 4th step: every ID you can still get, with the frames to wait from the 4th press. Pick one: "
                          "the countdown aims at the middle of its window."));
    layout->addWidget(table_, 1);

    auto *cal = new QGroupBox(tr("Calibration"));
    auto *calForm = new QFormLayout(cal);
    pressMs_ = doubleSpin(tr(" ms"));
    pressMs_->setObjectName("doubleSpinBoxPressMs");
    pressMs_->setToolTip(tr("Added to the A press. Update corrects it from the TID you got."));
    blinkOffset_ = spin(-600, 600, tr(" frames"));
    blinkOffset_->setObjectName("spinBoxBlinkOffset");
    blinkOffset_->setToolTip(tr("Faster/Slower applied when the beeps start (Faster and Slower change it as you go)"));
    calForm->addRow(tr("A press"), pressMs_);
    calForm->addRow(tr("Blink offset"), blinkOffset_);
    tidHit_ = hitEdit(tr("TID you got"));
    tidHit_->setObjectName("lineEditTidHit");
    calForm->addRow(tr("Hit"), tidHit_);
    layout->addWidget(cal);
    watch(pressMs_, true);
    watch(blinkOffset_, true);

    connect(paste_, &QPushButton::clicked, this, [this] {
        auto h = XYTid::Handoff::fromString(QApplication::clipboard()->text().trimmed().toStdString());
        if (!h)
        {
            emit message(tr("The clipboard doesn't hold PokeFinderPlus' XY TID details (\"Copy for PokeTimerPlus\")."));
        }
        else if (!load(*h))
        {
            emit message(tr("Stop the running timeline first."));
        }
    });
    connect(faster_, &QPushButton::clicked, this, [this] {
        if (session_)
        {
            session_->nudge(1);
            nudges_++;
            blinkOffset_->setValue(blinkOffset_->value() + 1);
            lastRefresh_ = 0; // beeps again at the next tick
        }
    });
    connect(slower_, &QPushButton::clicked, this, [this] {
        if (session_)
        {
            session_->nudge(-1);
            nudges_--;
            blinkOffset_->setValue(blinkOffset_->value() - 1);
            lastRefresh_ = 0;
        }
    });
    connect(step_, &QPushButton::pressed, this, [this] { stepPressed(-1); });
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this] {
        auto rows = table_->selectionModel()->selectedRows();
        if (!rows.isEmpty())
        {
            chooseRow(rows.first().row());
        }
    });

    setRunning(false);
    updateStepLabel();
}

Fields XYTidPanel::calibration() const
{
    return { { "pressMs", pressMs_->value() }, { "blinkOffset", double(blinkOffset_->value()) } };
}

void XYTidPanel::setCalibration(const Fields &fields)
{
    loading_ = true;
    pressMs_->setValue(field(fields, "pressMs", 0));
    blinkOffset_->setValue(int(std::lround(field(fields, "blinkOffset", 0))));
    loading_ = false;
}

bool XYTidPanel::calibrate(const Calibrator &)
{
    auto tid = hit(tidHit_);
    if (!tid || !lastAim_ || lastRows_.empty())
    {
        return false;
    }
    // The row you landed in: the one with that TID nearest to the aim
    int aimIndex = -1;
    for (size_t i = 0; i < lastRows_.size(); i++)
    {
        if (lastRows_[i].advance == lastAim_->advance)
        {
            aimIndex = int(i);
        }
    }
    int best = -1;
    for (int i = 0; i < int(lastRows_.size()); i++)
    {
        if (lastRows_[size_t(i)].id.tid == XYTid::u16(*tid) && std::abs(i - aimIndex) <= 30
            && (best < 0 || std::abs(i - aimIndex) < std::abs(best - aimIndex)))
        {
            best = i;
        }
    }
    if (best < 0 || aimIndex < 0)
    {
        emit message(tr("TID %1 isn't near the target in the last list: no change.").arg(int(*tid)));
        return false;
    }
    tidHit_->clear();
    if (best == aimIndex)
    {
        emit message(tr("That's the target: no change needed."));
        return true;
    }
    const XYTid::Row &got = lastRows_[size_t(best)];
    const double error = (got.wait + got.window / 2.0) - (lastAim_->wait + lastAim_->window / 2.0); // frames, + = late
    pressMs_->setValue(pressMs_->value() - error * msPerFrame());
    emit message(tr("You pressed about %1 frames %2. A press calibration: %3 ms.")
                     .arg(std::abs(error), 0, 'f', 0)
                     .arg(error > 0 ? tr("late") : tr("early"))
                     .arg(pressMs_->value(), 0, 'f', 1));
    return true;
}

void XYTidPanel::setRunning(bool running)
{
    paste_->setEnabled(!running);
    faster_->setEnabled(running);
    slower_->setEnabled(running);
    step_->setEnabled(running);
    boy_->setEnabled(!running || (session_ && session_->getSteps() < 4));
    girl_->setEnabled(boy_->isEnabled());
}

bool XYTidPanel::load(const XYTid::Handoff &handoff)
{
    if (session_)
    {
        return false;
    }
    handoff_ = handoff;
    const qint64 ago = QDateTime::currentMSecsSinceEpoch() - handoff.tapEpochMs;
    QString text = tr("%1 · first tap %2 (%3 ago)")
                       .arg(QChar(handoff.game))
                       .arg(QDateTime::fromMSecsSinceEpoch(handoff.tapEpochMs).toString("HH:mm:ss.zzz"))
                       .arg(minutes(double(ago)));
    if (handoff.target)
    {
        text += tr(" · target advance %1").arg(*handoff.target);
        if (handoff.tid && handoff.sid)
        {
            text += tr(" = TID %1 / SID %2").arg(*handoff.tid).arg(*handoff.sid);
        }
    }
    else
    {
        text += tr(" · no target (pick it in the list after step 4)");
    }
    source_->setText(text);
    table_->setRowCount(0);
    rows_.clear();
    chosen_ = -1;
    emit changed();
    return true;
}

bool XYTidPanel::hasHandoff() const
{
    return handoff_.has_value();
}

bool XYTidPanel::isSessionRunning() const
{
    return session_ != nullptr;
}

double XYTidPanel::frameTime(double frame) const
{
    return startMs_ + frame * msPerFrame();
}

XYTid::u32 XYTidPanel::frameAt(double ms) const
{
    return ms <= startMs_ ? 0 : XYTid::u32((ms - startMs_) / msPerFrame());
}

void XYTidPanel::startSession(double nowMs)
{
    if (!handoff_)
    {
        return;
    }
    const double elapsed = double(QDateTime::currentMSecsSinceEpoch() - handoff_->tapEpochMs);
    session_ = std::make_unique<XYTid::Session>(handoff_->state, std::max(0.0, elapsed));
    startMs_ = nowMs;
    step4Ms_ = 0;
    nudges_ = 0;
    rows_.clear();
    chosen_ = -1;
    lastAim_.reset();
    table_->setRowCount(0);
    const int offset = blinkOffset_->value();
    for (int i = 0; i < std::abs(offset); i++)
    {
        session_->nudge(offset > 0 ? 1 : -1);
    }
    if (elapsed > 3 * 3600000.0)
    {
        emit message(
            tr("The first tap was %1 h ago: was the game reset since? Tap and search again if so.").arg(elapsed / 3600000.0, 0, 'f', 1));
    }
    updateStepLabel();
    refreshBeeps(nowMs);
}

void XYTidPanel::stopSession()
{
    if (!rows_.empty())
    {
        lastRows_ = rows_;
    }
    session_.reset();
    emit beepsChanged({});
    emit countdown(std::numeric_limits<double>::quiet_NaN(), QString());
    updateStepLabel();
}

void XYTidPanel::stepPressed(double nowMs)
{
    if (!session_)
    {
        return;
    }
    if (nowMs < 0)
    {
        nowMs = Clock::nowMs();
    }
    const XYTid::u32 f = frameAt(nowMs);
    session_->advanceTo(f);
    const int step = session_->step(f, boy_->isChecked());
    if (step == 0)
    {
        return;
    }
    if (step == 4)
    {
        step4Ms_ = nowMs;
        boy_->setEnabled(false);
        girl_->setEnabled(false);
        emit beepsChanged({}); // the blinks are done; the countdown comes with the list
    }
    else
    {
        lastRefresh_ = 0;
    }
    updateStepLabel();
}

void XYTidPanel::tick(double nowMs)
{
    if (!session_)
    {
        return;
    }
    session_->advanceTo(frameAt(nowMs));
    if (session_->getSteps() >= 4)
    {
        if (rows_.empty() && session_->hasSnapshot())
        {
            refreshList();
            refreshBeeps(nowMs);
        }
        if (chosen_ >= 0)
        {
            const XYTid::Row &row = rows_[size_t(chosen_)];
            emit countdown(aimMs(row) - nowMs, tr("A for TID %1").arg(row.id.tid));
        }
        else
        {
            emit countdown(std::numeric_limits<double>::quiet_NaN(), rows_.empty() ? tr("Listing the IDs…") : tr("Pick an ID"));
        }
        // "now" row: the one whose window the A press would fall in
        const double since = (nowMs - step4Ms_) / msPerFrame();
        for (int i = 0; i < table_->rowCount(); i++)
        {
            const XYTid::Row &row = rows_[size_t(i)];
            if (since < row.wait + row.window)
            {
                table_->item(i, 7)->setText(minutes(aimMs(row) - nowMs));
                if (i > 0)
                {
                    table_->item(i - 1, 7)->setText(tr("passed"));
                }
                break;
            }
        }
        return;
    }
    if (nowMs - lastRefresh_ > 1000)
    {
        refreshBeeps(nowMs);
    }
    emit countdown(std::numeric_limits<double>::quiet_NaN(), tr("Blinks"));
}

void XYTidPanel::setCountdown(int count, int intervalMs)
{
    count_ = std::max(1, count);
    intervalMs_ = std::max(1, intervalMs);
}

double XYTidPanel::aimMs(const XYTid::Row &row) const
{
    return step4Ms_ + (row.wait + row.window / 2.0) * msPerFrame() + pressMs_->value();
}

void XYTidPanel::refreshBeeps(double nowMs)
{
    lastRefresh_ = nowMs;
    std::vector<double> times;
    if (!session_)
    {
        emit beepsChanged(times);
        return;
    }
    if (session_->getSteps() < 4)
    {
        const auto until = frameAt(nowMs + BEEP_HORIZON_MS);
        for (XYTid::u32 frame : session_->upcomingBlinks(until))
        {
            double t = frameTime(double(frame + XYTid::BEEP_DELAY));
            if (t > nowMs)
            {
                times.push_back(t);
            }
        }
    }
    else if (chosen_ >= 0)
    {
        const double aim = aimMs(rows_[size_t(chosen_)]);
        for (int k = count_ - 1; k >= 0; k--)
        {
            double t = aim - k * double(intervalMs_);
            if (t > nowMs)
            {
                times.push_back(t);
            }
        }
    }
    emit beepsChanged(times);
}

void XYTidPanel::refreshList()
{
    rows_ = XYTid::waitList(session_->getSnapshot(), session_->isBoy(), LIST_ROWS);
    table_->blockSignals(true);
    table_->setRowCount(int(rows_.size()));
    int target = -1;
    for (int i = 0; i < int(rows_.size()); i++)
    {
        const XYTid::Row &r = rows_[size_t(i)];
        const QStringList cells
            = { QString::number(r.advance), QString::number(r.id.tid), QString::number(r.id.sid), QString::number(r.id.tsv),
                QString::number(r.id.trv),  QString::number(r.wait),   QString::number(r.window), QString() };
        for (int c = 0; c < cells.size(); c++)
        {
            auto *item = new QTableWidgetItem(cells[c]);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(i, c, item);
        }
        if (handoff_ && handoff_->target && r.advance == *handoff_->target)
        {
            target = i;
        }
    }
    table_->blockSignals(false);

    if (handoff_ && handoff_->target)
    {
        if (target < 0)
        {
            emit message(rows_.empty() || *handoff_->target < rows_.front().advance
                             ? tr("Advance %1 has already passed: pick another ID in the list.").arg(*handoff_->target)
                             : tr("Advance %1 is taken by a blink (no ID there): pick another ID in the list.").arg(*handoff_->target));
        }
        else
        {
            table_->selectRow(target);
            table_->scrollToItem(table_->item(target, 0), QAbstractItemView::PositionAtCenter);
        }
    }
    else
    {
        emit message(tr("Pick the ID you want in the list."));
    }
}

void XYTidPanel::chooseRow(int row)
{
    if (row < 0 || row >= int(rows_.size()) || !session_)
    {
        return;
    }
    const double lead = count_ * double(intervalMs_);
    const double now = startMs_ + session_->getFrame() * msPerFrame();
    if (aimMs(rows_[size_t(row)]) < now + lead)
    {
        emit message(tr("Too soon for a countdown: pick a later ID."));
        return;
    }
    chosen_ = row;
    lastAim_ = rows_[size_t(row)];
    lastRows_ = rows_;
    refreshBeeps(now);
}

void XYTidPanel::updateStepLabel()
{
    if (!session_)
    {
        stepLabel_->setText(handoff_ ? tr("Start (Space) on the professor's blinking screen") : tr("Waiting for PokeFinderPlus"));
        offsetLabel_->clear();
        return;
    }
    const int done = session_->getSteps();
    stepLabel_->setText(done < 4 ? tr(stepNames[done]) : tr("Done: wait for the countdown, A on the last beep"));
    offsetLabel_->setText(nudges_ == 0 ? QString() : tr("%1%2 frames this time").arg(nudges_ > 0 ? "+" : "").arg(nudges_));
}
