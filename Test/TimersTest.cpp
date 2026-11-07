/*
 * This file is part of PokeTimerPlus
 * The port is checked number for number against EonTimer 3.x itself: Tools/eontimer_golden.py ran
 * the real Python code on 3,420 cases (9 frame-rate / precision / minimum-length setups, two of them
 * with exact .5 ties so the round-half-to-even rule is exercised).
 */

#include "TimersTest.hpp"

#include <Core/Timer/Calibrator.hpp>
#include <Core/Timer/FrameRate.hpp>
#include <Core/Timer/Timers.hpp>
#include <QTest>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>

using json = nlohmann::json;

namespace
{
    double value(const json &j)
    {
        if (j.is_string() && j.get<std::string>() == "inf")
        {
            return Timers::Infinity;
        }
        return j.get<double>();
    }

    bool same(double a, double b)
    {
        if (std::isinf(a) || std::isinf(b))
        {
            return a == b;
        }
        return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(b));
    }

    int failures = 0;
    int checks = 0;

    void expect(double expected, double actual, const std::string &what)
    {
        checks++;
        if (!same(expected, actual))
        {
            if (failures++ < 20)
            {
                qWarning("%s: expected %.12g, got %.12g", what.c_str(), expected, actual);
            }
        }
    }

    void expectPhases(const json &expected, const std::vector<double> &actual, const std::string &what)
    {
        checks++;
        if (expected.size() != actual.size())
        {
            if (failures++ < 20)
            {
                qWarning("%s: %zu phases expected, got %zu", what.c_str(), expected.size(), actual.size());
            }
            return;
        }
        for (size_t i = 0; i < actual.size(); i++)
        {
            expect(value(expected[i]), actual[i], what + "[" + std::to_string(i) + "]");
        }
    }

    std::optional<int> optionalInt(const json &j)
    {
        return j.is_null() ? std::nullopt : std::optional<int>(j.get<int>());
    }
}

void TimersTest::roundHalfEven()
{
    QCOMPARE(::roundHalfEven(0.5), 0.0);
    QCOMPARE(::roundHalfEven(1.5), 2.0);
    QCOMPARE(::roundHalfEven(2.5), 2.0);
    QCOMPARE(::roundHalfEven(-0.5), -0.0);
    QCOMPARE(::roundHalfEven(-1.5), -2.0);
    QCOMPARE(::roundHalfEven(2.4999999), 2.0);
    QCOMPARE(::roundHalfEven(2.5000001), 3.0);
}

void TimersTest::frameRates()
{
    std::set<std::string> ids;
    for (const auto &f : FrameRates::all())
    {
        QVERIFY2(ids.insert(f.id).second, f.id.c_str());
        QVERIFY(f.fps > 0);
        QVERIFY(!f.name.empty());
    }
    QVERIFY(!ids.contains(std::string(FrameRates::Custom)));
    QCOMPARE(FrameRates::fps("gba", 0), 59.7275);
    QCOMPARE(FrameRates::fps("nds-slot1", 0), 59.8261);
    QCOMPARE(FrameRates::fps("nds-slot2", 0), 59.6555);
    QCOMPARE(FrameRates::fps("custom", 61.5), 61.5);
    QCOMPARE(FrameRates::fps("unknown-id", 42.0), 42.0);
}

void TimersTest::eonTimerGolden()
{
    std::ifstream in(GOLDEN_JSON);
    QVERIFY2(in.good(), GOLDEN_JSON);
    json golden = json::parse(in);
    failures = 0;
    checks = 0;

    int ci = 0;
    for (const auto &cfg : golden["configs"])
    {
        const Calibrator cal(cfg["fps"].get<double>(), cfg["precision"].get<bool>(), cfg["minimumLength"].get<int>());
        const std::string tag = "config " + std::to_string(ci++) + " ";

        for (const auto &c : cfg["calibrator"])
        {
            double ms = c["ms"];
            int delays = c["delays"];
            double seconds = c["seconds"];
            expect(c["toDelays"], cal.toDelays(ms), tag + "toDelays");
            expect(c["toMilliseconds"], cal.toMilliseconds(delays), tag + "toMilliseconds");
            expect(c["calibrateToDelays"], cal.calibrateToDelays(ms), tag + "calibrateToDelays");
            expect(c["calibrateToMilliseconds"], cal.calibrateToMilliseconds(delays), tag + "calibrateToMilliseconds");
            expect(c["createCalibration"], cal.createCalibration(delays, seconds), tag + "createCalibration");
        }
        for (const auto &c : cfg["second"])
        {
            expectPhases(c["phases"], Timers::secondPhases(cal, c["target"], c["calibration"]), tag + "second");
            expect(c["calibrate"], Timers::secondCalibration(c["target"], c["hit"]), tag + "secondCalibration");
        }
        for (const auto &c : cfg["delay"])
        {
            expectPhases(c["phases"], Timers::delayPhases(cal, c["target"], c["second"], c["calibration"]), tag + "delay");
            expect(c["calibrate"], Timers::delayCalibration(cal, c["target"], c["hit"]), tag + "delayCalibration");
        }
        for (const auto &c : cfg["frame"])
        {
            expectPhases(c["phases"], Timers::framePhases(cal, c["preTimer"], c["target"], c["calibration"]), tag + "frame");
            expect(c["calibrate"], Timers::frameCalibration(cal, c["target"], c["hit"]), tag + "frameCalibration");
            expectPhases(c["variable"], Timers::variableFramePhases(c["preTimer"]), tag + "variableFrame");
        }
        for (const auto &c : cfg["entralink"])
        {
            expectPhases(c["phases"],
                         Timers::entralinkPhases(cal, c["target"], c["second"], c["calibration"], c["entralinkCalibration"]),
                         tag + "entralink");
            expect(c["calibrate"], Timers::delayCalibration(cal, c["target"], c["hit"]), tag + "entralinkCalibration");
        }
        for (const auto &c : cfg["enhanced"])
        {
            expectPhases(c["phases"],
                         Timers::enhancedEntralinkPhases(cal, c["target"], c["second"], c["advances"], c["calibration"],
                                                         c["entralinkCalibration"], c["frameCalibration"]),
                         tag + "enhancedEntralink");
            expect(c["calibrate"], Timers::enhancedEntralinkCalibration(c["advances"], c["advancesHit"]), tag + "enhancedCalibration");
        }
        for (const auto &c : cfg["gen3"])
        {
            Gen3Targets t;
            t.mode = gen3ModeFromString(c["mode"].get<std::string>()).value();
            t.preTimer = c["preTimer"];
            t.targetFrame = c["targetFrame"];
            double calibration = c["calibration"];
            expectPhases(c["phases"], Timers::gen3Phases(cal, t, calibration), tag + "gen3");
            expect(c["calibrationAfter"], calibration + Timers::gen3CalibrationDelta(cal, t, c["frameHit"]), tag + "gen3Calibrate");
        }
        for (const auto &c : cfg["gen4"])
        {
            Gen4Targets t { c["targetDelay"], c["targetSecond"] };
            Gen4Calibration k { c["calibratedDelay"], c["calibratedSecond"] };
            expect(c["calibration"], Timers::gen4Calibration(cal, k), tag + "gen4Calibration");
            expectPhases(c["phases"], Timers::gen4Phases(cal, t, k), tag + "gen4");
            expect(c["minutesBefore"], Timers::gen4MinutesBeforeTarget(cal, t), tag + "gen4MinutesBefore");
            expect(c["calibratedDelayAfter"], k.calibratedDelay + Timers::gen4CalibratedDelayDelta(cal, t, c["delayHit"]),
                   tag + "gen4Calibrate");
        }
        for (const auto &c : cfg["gen5"])
        {
            Gen5Targets t;
            t.mode = gen5ModeFromString(c["mode"].get<std::string>()).value();
            t.targetDelay = c["targetDelay"];
            t.targetSecond = c["targetSecond"];
            t.targetAdvances = c["targetAdvances"];
            Gen5Calibration k { c["calibration"], c["entralinkCalibration"], c["frameCalibration"].get<double>() };
            expectPhases(c["phases"], Timers::gen5Phases(cal, t, k), tag + "gen5 " + c["mode"].get<std::string>());
            expect(c["minutesBefore"], Timers::gen5MinutesBeforeTarget(cal, t), tag + "gen5MinutesBefore");
            Gen5Hits hits { optionalInt(c["delayHit"]), optionalInt(c["secondHit"]), optionalInt(c["advancesHit"]) };
            Gen5Calibration after = Timers::gen5Calibrate(cal, t, k, hits);
            expect(c["calibrationAfter"], after.calibration, tag + "gen5Calibrate.calibration");
            expect(c["entralinkCalibrationAfter"], after.entralink, tag + "gen5Calibrate.entralink");
            expect(c["frameCalibrationAfter"], after.frame, tag + "gen5Calibrate.frame");
        }
        for (const auto &c : cfg["custom"])
        {
            std::vector<CustomPhase> phases;
            for (const auto &p : c["phases"])
            {
                phases.push_back({ customUnitFromString(p["unit"].get<std::string>()).value(), p["value"], p["calibration"] });
            }
            expectPhases(c["result"], Timers::customPhases(cal, phases), tag + "custom");
        }
    }
    qInfo("EonTimer golden: %d checks, %d mismatches", checks, failures);
    QCOMPARE(failures, 0);
    QVERIFY(checks > 4000);
}

// The upstream EonTimer unit tests (1 ms per frame), ported as-is.
void TimersTest::eonTimerUnitCases()
{
    const Calibrator cal(1000.0);
    QCOMPARE(Timers::delayPhases(cal, 600, 50, 0), (std::vector<double> { 49600, 600 }));
    QCOMPARE(Timers::delayCalibration(cal, 0, 0), 0.0);
    QCOMPARE(Timers::delayCalibration(cal, 0, 1), 0.75);
    QCOMPARE(Timers::delayCalibration(cal, 0, 167), 125.25);
    QCOMPARE(Timers::delayCalibration(cal, 0, 168), 168.0);
    QCOMPARE(Timers::secondPhases(cal, 50, 0), (std::vector<double> { 50200 }));
    QCOMPARE(Timers::secondPhases(cal, 5, 0), (std::vector<double> { 65200 })); // padded to the 14 s minimum
    QCOMPARE(Timers::secondCalibration(50, 49), 500.0);
    QCOMPARE(Timers::secondCalibration(50, 51), -500.0);
    QCOMPARE(Timers::framePhases(cal, 5000, 1000, 0), (std::vector<double> { 5000, 1000 }));
    QCOMPARE(Timers::frameCalibration(cal, 1000, 1010), -10.0);
    QVERIFY(std::isinf(Timers::variableFramePhases(5000)[1]));
    QCOMPARE(Timers::minutesBeforeTarget({ 65200, Timers::Infinity, 60000 }), 2);
}
