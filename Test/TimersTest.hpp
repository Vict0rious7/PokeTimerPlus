/*
 * This file is part of PokeTimerPlus
 */

#ifndef TIMERSTEST_HPP
#define TIMERSTEST_HPP

#include <QObject>

class TimersTest : public QObject
{
    Q_OBJECT
private slots:
    void roundHalfEven();
    void frameRates();
    void eonTimerGolden();
    void eonTimerUnitCases();
};

#endif // TIMERSTEST_HPP
