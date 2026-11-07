/*
 * This file is part of PokeTimerPlus
 * The countdown: current phase (big), next phase, minutes before target. Flashes on each beep.
 */

#ifndef TIMERDISPLAY_HPP
#define TIMERDISPLAY_HPP

#include <QWidget>

class QLabel;
class QTimer;

class TimerDisplay : public QWidget
{
    Q_OBJECT
public:
    explicit TimerDisplay(QWidget *parent = nullptr);

    void showPhases(double current, double next); // idle preview
    void showProgress(int phase, double remaining, double next);
    void showContinuous(double elapsed, double toNext, int targetsLeft);
    void setMinutesBefore(int minutes); // -1 hides it
    void flash();

    static QString format(double ms); // "35.520", "∞", "1:05.200"

private:
    QLabel *current_;
    QLabel *next_;
    QLabel *phase_;
    QLabel *minutes_;
    QTimer *flashTimer_;
};

#endif // TIMERDISPLAY_HPP
