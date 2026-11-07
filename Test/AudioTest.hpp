/*
 * This file is part of PokeTimerPlus
 */

#ifndef AUDIOTEST_HPP
#define AUDIOTEST_HPP

#include <QObject>

class AudioTest : public QObject
{
    Q_OBJECT
private slots:
    void steadyStream();
    void stallIsCorrected();
    void driftIsCorrected();
    void countdownSpacingIsKept();
    void callbackJitterIsIgnored();
    void flowingScheduleChanges();
    void wavParsing();
};

#endif // AUDIOTEST_HPP
