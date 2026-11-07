/*
 * This file is part of PokeTimerPlus
 */

#include "ScheduleTest.hpp"

#include <Core/Timer/Calibrator.hpp>
#include <Core/Timer/Schedule.hpp>
#include <Core/Timer/Timers.hpp>
#include <QTest>

namespace
{
    std::vector<double> times(const std::vector<Beep> &beeps)
    {
        std::vector<double> out;
        for (const auto &b : beeps)
        {
            out.push_back(b.ms);
        }
        return out;
    }
}

void ScheduleTest::beepsAtPhaseEnds()
{
    Schedule s({ 10000, 3000 }, 3, 500);
    QCOMPARE(times(s.beeps()), (std::vector<double> { 9000, 9500, 10000, 12000, 12500, 13000 }));
    auto b = s.beeps();
    QCOMPARE(b[2].phase, 0);
    QCOMPARE(b[2].fromEnd, 0);
    QCOMPARE(b[3].phase, 1);
    QCOMPARE(b[3].fromEnd, 2);
    QCOMPARE(s.totalMs(), 13000.0);
}

void ScheduleTest::shortPhaseGetsFewerBeeps()
{
    // EonTimer: only beeps with k * interval < phase length
    Schedule s({ 1200 }, 6, 500);
    QCOMPARE(times(s.beeps()), (std::vector<double> { 200, 700, 1200 }));
    Schedule exact({ 1000 }, 6, 500);
    QCOMPARE(times(exact.beeps()), (std::vector<double> { 500, 1000 }));
}

void ScheduleTest::openPhaseStopsBeeps()
{
    Schedule s(Timers::variableFramePhases(5000), 3, 500);
    QCOMPARE(times(s.beeps()), (std::vector<double> { 4000, 4500, 5000 }));
    QVERIFY(std::isinf(s.totalMs()));
    QVERIFY(std::isinf(s.phaseEnd(1)));
}

void ScheduleTest::flowingTarget()
{
    // Gen 3 Variable Target / FlowTimer variable offset: the frame is typed while the timer runs.
    const Calibrator cal(59.7275);
    Schedule s(Timers::variableFramePhases(5000), 4, 500);
    const double target = Timers::framePhase(cal, 600, 0); // 600 frames ~ 10,046 ms
    QVERIFY(s.setPhase(1, target, 7000));
    auto after = s.beepsAfter(7000);
    QCOMPARE(after.size(), size_t(4));
    QCOMPARE(after.back().ms, 5000 + target);
    QCOMPARE(after.back().fromEnd, 0);
    // a target that has already passed is reported
    QVERIFY(!s.setPhase(1, 1000, 7000));
    QVERIFY(s.beepsAfter(7000).empty());
}

void ScheduleTest::appendTarget()
{
    Schedule s({ 5000 }, 2, 500);
    s.appendPhase(3000);
    QCOMPARE(times(s.beeps()), (std::vector<double> { 4500, 5000, 7500, 8000 }));
}

void ScheduleTest::phaseAt()
{
    Schedule s({ 5000, 3000 }, 1, 500);
    QCOMPARE(s.phaseAt(0), 0);
    QCOMPARE(s.phaseAt(4999.9), 0);
    QCOMPARE(s.phaseAt(5000), 1);
    QCOMPARE(s.phaseAt(9000), 2);
}
