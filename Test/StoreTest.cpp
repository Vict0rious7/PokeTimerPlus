/*
 * This file is part of PokeTimerPlus
 */

#include "StoreTest.hpp"

#include <Core/Profile/CalibrationStore.hpp>
#include <Core/Timer/FrameRate.hpp>
#include <Core/Timer/RngTypes.hpp>
#include <Core/Timer/Schedule.hpp>
#include <QTemporaryDir>
#include <QTest>
#include <set>

void StoreTest::catalog()
{
    std::set<std::string> ids, keys;
    for (const auto &t : RngTypes::all())
    {
        QVERIFY2(ids.insert(t.id).second, t.id.c_str());
        QVERIFY(!t.sections.empty());
        bool consoleKnown = false;
        for (const auto &c : RngTypes::consoles())
        {
            consoleKnown = consoleKnown || c.id == t.console;
        }
        QVERIFY2(consoleKnown, t.id.c_str());
        for (const auto &s : t.sections)
        {
            keys.insert(RngTypes::calibrationKey(t, s));
        }
    }
    // Every console tab ends with a Custom timer, and its frame rates exist.
    for (const auto &c : RngTypes::consoles())
    {
        auto list = RngTypes::forConsole(c.id);
        QVERIFY(!list.empty());
        QVERIFY(list.back()->timer == TimerTemplate::Custom);
        for (const auto &f : c.frameRates)
        {
            QVERIFY2(FrameRates::find(f), f.c_str());
        }
    }
    // Victor's list
    for (const char *id : { "rse-wild", "rse-static", "rse-fishing", "e-egg", "rs-egg", "frlg-static", "frlg-wild", "frlg-egg",
                            "dppt-static", "dppt-wild", "hgss-static", "hgss-wild", "bw" })
    {
        QVERIFY2(RngTypes::find(id), id);
    }
    QVERIFY(RngTypes::find("frlg-static")->timer == TimerTemplate::SeedAdvances);
    QVERIFY(RngTypes::find("e-egg")->sections.size() == 2);
    QVERIFY(keys.contains("e-egg/held") && keys.contains("e-egg/pickup") && keys.contains("frlg-egg/held"));
    QCOMPARE(RngTypes::find("bw")->calibrationKey, std::string("gen5")); // shared by all Gen 5
    QVERIFY(RngTypes::find("hgss-static")->timer == TimerTemplate::Gen4Delay); // same timer as DPPt, own calibration
    QVERIFY(RngTypes::find("hgss-wild")->calibrationKey != RngTypes::find("dppt-wild")->calibrationKey);
    QVERIFY(!RngTypes::find("e-egg")->sections[0].note.empty()); // the shiny / IV hint
}

void StoreTest::roundTrip()
{
    CalibrationStore s;
    s.set("e-egg/held", { { "ms", 167.43 } });
    s.set("frlg-static", { { "seedMs", -99 }, { "advances", 1616 } });
    s.customPhases["custom-3ds"] = { { CustomUnit::Advances, 1200, -3.5 }, { CustomUnit::Milliseconds, 5000, 0 } };
    s.hardware["gba"] = "nds-slot2";
    s.customFps["switch"] = 59.5;
    CalibrationStore t = CalibrationFile::fromJson(CalibrationFile::toJson(s));
    QCOMPARE(t.get("e-egg/held", "ms").value(), 167.43);
    QCOMPARE(t.get("frlg-static", "advances").value(), 1616.0);
    QVERIFY(!t.get("frlg-static", "nothing"));
    QVERIFY(!t.has("rse-wild"));
    QCOMPARE(t.customPhases["custom-3ds"].size(), size_t(2));
    QVERIFY(t.customPhases["custom-3ds"][0].unit == CustomUnit::Advances);
    QCOMPARE(t.customPhases["custom-3ds"][0].calibration, -3.5);
    QCOMPARE(t.hardware["gba"], std::string("nds-slot2"));
    QCOMPARE(t.customFps["switch"], 59.5);
}

void StoreTest::tolerantParsing()
{
    std::string error;
    CalibrationStore broken = CalibrationFile::fromJson("{ nope", &error);
    QVERIFY(!error.empty());
    QVERIFY(broken.calibrations.empty());
    CalibrationStore odd = CalibrationFile::fromJson(
        R"({"calibrations":{"a":{"ms":"x","ok":2},"b":3},"customPhases":{"c":[{"unit":"Bad","target":"q"}]},"customFps":{"gba":-1},"future":1})");
    QVERIFY(!odd.get("a", "ms"));
    QCOMPARE(odd.get("a", "ok").value(), 2.0);
    QVERIFY(!odd.has("b"));
    QCOMPARE(odd.customPhases["c"].size(), size_t(1));
    QVERIFY(odd.customPhases["c"][0].unit == CustomUnit::Milliseconds);
    QVERIFY(odd.customFps.empty());
}

void StoreTest::saveAndLoad()
{
    QTemporaryDir dir;
    const std::string path = dir.filePath("x/calibrations.json").toStdString();
    QVERIFY(CalibrationFile::load(path).calibrations.empty());
    CalibrationStore s;
    s.set("gen5", { { "calibration", -95 }, { "entralink", 256 }, { "frame", 0 } });
    std::string error;
    QVERIFY2(CalibrationFile::save(path, s, &error), error.c_str());
    QCOMPARE(CalibrationFile::load(path).get("gen5", "calibration").value(), -95.0);
}

void StoreTest::continuousTargets()
{
    auto b = beepsForTargets({ 10000, 2000 }, 3, 500);
    std::vector<double> ms;
    for (const auto &x : b)
    {
        ms.push_back(x.ms);
    }
    QCOMPARE(ms, (std::vector<double> { 1000, 1500, 2000, 9000, 9500, 10000 }));
    QCOMPARE(b[2].fromEnd, 0);
    QCOMPARE(b[2].phase, 1);
    auto early = beepsForTargets({ 600 }, 3, 500); // the first beep would be before the start
    QCOMPARE(early.size(), size_t(2));
}
