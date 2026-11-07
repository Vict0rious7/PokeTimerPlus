/*
 * This file is part of PokeTimerPlus
 */

#ifndef SCHEDULETEST_HPP
#define SCHEDULETEST_HPP

#include <QObject>

class ScheduleTest : public QObject
{
    Q_OBJECT
private slots:
    void beepsAtPhaseEnds();
    void shortPhaseGetsFewerBeeps();
    void openPhaseStopsBeeps();
    void flowingTarget();
    void appendTarget();
    void phaseAt();
};

#endif // SCHEDULETEST_HPP
