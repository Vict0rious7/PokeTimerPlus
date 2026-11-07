/*
 * This file is part of PokeTimerPlus
 */

#include "TimerDisplay.hpp"

#include <QFontDatabase>
#include <QLabel>
#include <QPalette>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

TimerDisplay::TimerDisplay(QWidget *parent) : QWidget(parent), flashTimer_(new QTimer(this))
{
    auto *layout = new QVBoxLayout(this);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    current_ = new QLabel(QStringLiteral("0.000"));
    QFont big = mono;
    big.setPointSize(42);
    big.setBold(true);
    current_->setFont(big);
    current_->setAlignment(Qt::AlignCenter);
    current_->setMinimumWidth(QFontMetrics(big).horizontalAdvance(QStringLiteral("00:00.000")) + 20);

    phase_ = new QLabel(tr("Ready"));
    phase_->setAlignment(Qt::AlignCenter);

    next_ = new QLabel;
    QFont mid = mono;
    mid.setPointSize(16);
    next_->setFont(mid);
    next_->setAlignment(Qt::AlignCenter);

    minutes_ = new QLabel;
    minutes_->setAlignment(Qt::AlignCenter);

    layout->addStretch();
    layout->addWidget(phase_);
    layout->addWidget(current_);
    layout->addWidget(next_);
    layout->addWidget(minutes_);
    layout->addStretch();

    setAutoFillBackground(true);
    flashTimer_->setSingleShot(true);
    flashTimer_->setInterval(110);
    connect(flashTimer_, &QTimer::timeout, this, [this] { setPalette(QPalette()); });
}

QString TimerDisplay::format(double ms)
{
    if (std::isnan(ms))
    {
        return QStringLiteral("–");
    }
    if (std::isinf(ms))
    {
        return QStringLiteral("∞");
    }
    ms = std::max(0.0, ms);
    const qint64 total = qint64(std::floor(ms));
    const qint64 minutes = total / 60000;
    const qint64 seconds = (total / 1000) % 60;
    const qint64 millis = total % 1000;
    if (minutes > 0)
    {
        return QStringLiteral("%1:%2.%3").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0')).arg(millis, 3, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1.%2").arg(seconds).arg(millis, 3, 10, QLatin1Char('0'));
}

void TimerDisplay::showPhases(double current, double next)
{
    phase_->setText(tr("Ready"));
    current_->setText(format(current));
    next_->setText(std::isnan(next) ? QString() : tr("Next: %1").arg(format(next)));
}

void TimerDisplay::showProgress(int phase, double remaining, double next)
{
    phase_->setText(tr("Phase %1").arg(phase + 1));
    current_->setText(format(remaining));
    next_->setText(std::isnan(next) ? QString() : tr("Next: %1").arg(format(next)));
}

void TimerDisplay::showContinuous(double elapsed, double toNext, int targetsLeft)
{
    if (std::isnan(toNext))
    {
        phase_->setText(tr("Running: add a target"));
        current_->setText(format(elapsed));
        next_->clear();
    }
    else
    {
        phase_->setText(targetsLeft > 1 ? tr("Next target (%1 left)").arg(targetsLeft) : tr("Next target"));
        current_->setText(format(toNext));
        next_->setText(tr("Elapsed: %1").arg(format(elapsed)));
    }
}

void TimerDisplay::setMinutesBefore(int minutes)
{
    minutes_->setVisible(minutes >= 0);
    minutes_->setText(tr("Minutes before target: %1").arg(minutes));
}

void TimerDisplay::flash()
{
    QPalette p = palette();
    p.setColor(QPalette::Window, palette().color(QPalette::Highlight));
    p.setColor(QPalette::WindowText, palette().color(QPalette::HighlightedText));
    setPalette(p);
    flashTimer_->start();
}
