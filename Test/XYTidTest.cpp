/*
 * This file is part of PokeTimerPlus
 * XY TID timeline checked against 6genTidSearch's own code (BZL, used with permission).
 */

#include "XYTidTest.hpp"
#include <Core/Gen6/XYTid.hpp>
#include <QTest>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace XYTid;

namespace
{
    json golden;

    std::array<u32, 4> words(const json &j)
    {
        return { j[0].get<u32>(), j[1].get<u32>(), j[2].get<u32>(), j[3].get<u32>() };
    }
}

void XYTidTest::initTestCase()
{
    std::ifstream in(XYTID_JSON);
    QVERIFY2(in.good(), XYTID_JSON);
    golden = json::parse(in);
}

void XYTidTest::tinyMT()
{
    int checks = 0;
    for (const auto &c : golden["tinymt"])
    {
        Tiny mt = Tiny::fromSeed(c["seed"].get<u32>());
        QCOMPARE(mt.s, words(c["status"]));
        for (const auto &out : c["out"])
        {
            QCOMPARE(mt.next(), out.get<u32>());
            checks++;
        }
    }
    for (const auto &c : golden["calcblink"])
    {
        QCOMPARE(blinkInterval(c[0].get<u32>()), c[1].get<u32>());
        checks++;
    }
    QVERIFY(checks > 15000);
}

void XYTidTest::timeline()
{
    int rows = 0;
    for (const auto &c : golden["timeline"])
    {
        bool boy = c["male"].get<bool>();
        Session session(words(c["state"]), c["elapsedMs"].get<u32>());
        std::vector<u32> blinks;
        for (const auto &n : c["nudges"])
        {
            session.advanceTo(n[0].get<u32>() - 1, &blinks);
            session.nudge(n[1].get<int>());
        }
        for (int i = 0; i < 4; i++)
        {
            u32 press = c["steps"][i].get<u32>();
            session.advanceTo(press, &blinks);
            QCOMPARE(session.step(press, boy), i + 1);
        }
        session.advanceTo(c["steps"][3].get<u32>() + (boy ? STEP4_RESET_BOY : STEP4_RESET_GIRL), &blinks);
        QVERIFY(session.hasSnapshot());
        QCOMPARE(session.getSnapshot().getState(), words(c["snapshot"]["seed"]));

        const json &expected = c["blinks"];
        for (size_t i = 0; i < expected.size(); i++)
        {
            QCOMPARE(blinks[i], expected[i].get<u32>());
        }
        // upcomingBlinks must match what running the session gives
        Session fresh(words(c["state"]), c["elapsedMs"].get<u32>());
        std::vector<u32> ahead = fresh.upcomingBlinks(c["steps"][0].get<u32>());
        std::vector<u32> ran;
        fresh.advanceTo(c["steps"][0].get<u32>(), &ran);
        QCOMPARE(ahead, ran);

        const json &list = c["rows"];
        std::vector<Row> got = waitList(session.getSnapshot(), boy, static_cast<u32>(list.size()));
        for (size_t i = 0; i < got.size(); i++)
        {
            const json &r = list[i];
            QCOMPARE(got[i].offset, r[0].get<u32>());
            QCOMPARE(got[i].id.tid, r[1].get<u16>());
            QCOMPARE(got[i].id.sid, r[2].get<u16>());
            QCOMPARE(got[i].wait, r[5].get<u32>());
            QCOMPARE(got[i].window, r[6].get<u32>());
            rows++;
        }
    }
    QVERIFY(rows >= 40000);
}

void XYTidTest::nudges()
{
    std::array<u32, 4> st = { 1, 2, 3, 4 };
    // Three "slower" at once = three frames not processed (one each), as three single ones would be
    Session a(st, 10000), b(st, 10000);
    a.advanceTo(100);
    b.advanceTo(100);
    a.nudge(-1);
    a.nudge(-1);
    a.nudge(-1);
    a.advanceTo(2000);
    for (u32 f = 100; f < 103; f++)
    {
        b.nudge(-1);
        b.advanceTo(f + 1);
    }
    b.advanceTo(2000);
    QCOMPARE(a.getTimeline().getState(), b.getTimeline().getState());
    QCOMPARE(a.getTimeline().getCounters(), b.getTimeline().getCounters());
    // Two "faster" = the timeline two frames ahead
    Session c(st, 10000), d(st, 10000);
    c.advanceTo(100);
    d.advanceTo(100);
    c.nudge(1);
    c.nudge(1);
    c.advanceTo(2000);
    d.advanceTo(2002);
    QCOMPARE(c.getTimeline().getCounters(), d.getTimeline().getCounters());
}

void XYTidTest::handoff()
{
    auto h = Handoff::fromString("PokeTimerPlus XYTID 1 game=Y state=FFFFFFFF,00000003,ABCDEF01,00000001 tap=1794482303966 target=165 "
                                 "tid=12345 sid=54321");
    QVERIFY(h.has_value());
    QCOMPARE(h->game, 'Y');
    QCOMPARE(h->state, (std::array<u32, 4> { 1, 0xabcdef01, 3, 0xffffffff }));
    QCOMPARE(h->tapEpochMs, 1794482303966LL);
    QCOMPARE(*h->target, 165u);
    QCOMPARE(Handoff::fromString(h->toString())->toString(), h->toString());
    QVERIFY(!Handoff::fromString("PokeTimerPlus XYTID 2 state=1,2,3,4 tap=5").has_value());
}
