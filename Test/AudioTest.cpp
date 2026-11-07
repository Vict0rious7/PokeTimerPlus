/*
 * This file is part of PokeTimerPlus
 * BeepMixer against a simulated sound card: callbacks every 10 ms of a fake system clock, a fixed
 * 30 ms output buffer, and optional stalls, clock drift and callback jitter. We check when each beep
 * would be heard, which is what the RNG cares about.
 */

#include "AudioTest.hpp"

#include <Core/Audio/BeepMixer.hpp>
#include <Core/Audio/Wav.hpp>
#include <QTest>
#include <cmath>
#include <random>

namespace
{
    struct SimCard
    {
        int rate = 48000;
        int block = 480; // 10 ms
        double bufferMs = 30;
        double ppm = 0; // device clock vs system clock (positive = device runs fast)
        double jitterMs = 0; // random callback lateness, 0..jitterMs
        double stallAtMs = -1; // system time of a stall (device stops consuming)
        double stallMs = 0;
        unsigned seed = 1;

        double now = 1000; // fake system clock
        BeepMixer mixer { [this] { return now; } };
        std::vector<std::pair<double, int64_t>> heardBlocks; // (heard system time of block start, block frame)
        std::vector<float> lastOut;
        std::vector<double> beepsHeard; // heard system time of each beep onset

        // Runs until system time `until`, recording beep onsets.
        void run(double until)
        {
            std::mt19937 rng(seed);
            std::uniform_real_distribution<double> late(0, jitterMs);
            while (true)
            {
                const double blockMs = block * 1000.0 / (rate * (1 + ppm * 1e-6));
                // nominal time of this callback on the device clock
                double due = start + double(blocks) * blockMs + shift;
                if (stallAtMs >= 0 && !stalled && due >= stallAtMs)
                {
                    shift += stallMs;
                    stalled = true;
                    due += stallMs;
                }
                if (due > until)
                {
                    break;
                }
                now = due + (jitterMs > 0 ? late(rng) : 0);
                lastOut.assign(size_t(block) * 2, 0.0f);
                mixer.render(lastOut.data(), block);
                // frames of this block are heard bufferMs after the block's nominal time
                const double heardStart = due + bufferMs;
                for (int i = 0; i < block; i++)
                {
                    const bool on = std::abs(lastOut[size_t(i) * 2]) > 1e-4f;
                    if (on && !wasSounding)
                    {
                        beepsHeard.push_back(heardStart + i * blockMs / block);
                    }
                    wasSounding = on;
                }
                blocks++;
            }
        }

        void begin()
        {
            start = now;
            mixer.configure(rate, 2, bufferMs);
            // a 20 ms beep, loud from its first sample
            std::vector<float> beep(size_t(rate) / 50, 0.5f);
            mixer.setSound(beep, rate);
        }

        double start = 0;
        double shift = 0;
        bool stalled = false;
        int64_t blocks = 0;
        bool wasSounding = false;
    };

    double worstError(const std::vector<double> &heard, const std::vector<double> &wanted)
    {
        if (heard.size() != wanted.size())
        {
            return 1e9;
        }
        double w = 0;
        for (size_t i = 0; i < heard.size(); i++)
        {
            w = std::max(w, std::abs(heard[i] - wanted[i]));
        }
        return w;
    }
}

void AudioTest::steadyStream()
{
    SimCard card;
    card.begin();
    card.run(1500); // stream running before the timer starts, as in the app
    std::vector<double> wanted = { 3000, 3500, 4000, 12345.6 };
    card.mixer.setSchedule(wanted);
    card.run(13000);
    QVERIFY2(worstError(card.beepsHeard, wanted) < 0.05, qPrintable(QString::number(worstError(card.beepsHeard, wanted))));
    QCOMPARE(card.mixer.slipCount(), 0);
}

void AudioTest::stallIsCorrected()
{
    SimCard card;
    card.stallAtMs = 2000;
    card.stallMs = 40;
    card.begin();
    card.run(1500);
    std::vector<double> wanted = { 4000, 4500, 5000 };
    card.mixer.setSchedule(wanted);
    card.run(6000);
    QVERIFY2(worstError(card.beepsHeard, wanted) < 0.5, qPrintable(QString::number(worstError(card.beepsHeard, wanted))));
    QCOMPARE(card.mixer.slipCount(), 1);
    QVERIFY(std::abs(card.mixer.correctedMs() - 40) < 1);
}

void AudioTest::driftIsCorrected()
{
    // 200 ppm (far more than the 2.5-5 ppm measured on the MacBook Air): 6 ms off after 30 s uncorrected.
    SimCard card;
    card.ppm = -200;
    card.begin();
    card.run(1500);
    std::vector<double> wanted = { 31000, 31500 };
    card.mixer.setSchedule(wanted);
    card.run(32000);
    QVERIFY2(worstError(card.beepsHeard, wanted) < 1.3, qPrintable(QString::number(worstError(card.beepsHeard, wanted))));
    QCOMPARE(card.mixer.slipCount(), 0);
}

void AudioTest::countdownSpacingIsKept()
{
    // Drift fixes wait for a gap: inside a countdown the spacing stays exact, even when the drift
    // crosses the 1 ms threshold mid-countdown.
    SimCard card;
    card.ppm = -200;
    card.begin();
    card.run(1500);
    std::vector<double> wanted;
    for (int i = 0; i < 12; i++)
    {
        wanted.push_back(6000 + i * 500.0); // 5.5 s of beeps every 500 ms
    }
    card.mixer.setSchedule(wanted);
    card.run(12500);
    QCOMPARE(card.beepsHeard.size(), wanted.size());
    for (size_t i = 1; i < wanted.size(); i++)
    {
        const double spacing = card.beepsHeard[i] - card.beepsHeard[i - 1];
        // the device clock itself is 200 ppm slow: 500 ms of its samples last 500.1 ms
        QVERIFY2(std::abs(spacing - 500 * (1 + 200e-6)) < 0.03, qPrintable(QString::number(spacing, 'f', 4)));
    }
}

void AudioTest::callbackJitterIsIgnored()
{
    // Callbacks up to 6 ms late (thread scheduling) must not be taken for stalls.
    SimCard card;
    card.jitterMs = 6;
    card.begin();
    card.run(1500);
    std::vector<double> wanted = { 5000, 5500, 9000 };
    card.mixer.setSchedule(wanted);
    card.run(10000);
    // what is left is the lateness of the least-late callback in the 250 ms calibration window
    QVERIFY2(worstError(card.beepsHeard, wanted) < 0.6, qPrintable(QString::number(worstError(card.beepsHeard, wanted))));
    QCOMPARE(card.mixer.slipCount(), 0);
    QVERIFY(std::abs(card.mixer.correctedMs()) < 1e-9); // no drift/slip correction after the start
}

void AudioTest::flowingScheduleChanges()
{
    SimCard card;
    card.begin();
    card.run(1500);
    card.mixer.setSchedule({ 3000, 3500 });
    card.run(3005); // beep at 3000 is sounding (20 ms long)
    // New target typed while running: one beep due 3 ms ago (plays at once), one 50 ms ago (dropped),
    // one in the future. The beep already sounding must not be cut or repeated.
    // The next block (next callback + 30 ms buffer) is heard at now + 40.
    card.mixer.setSchedule({ card.now + 40 - 3, card.now + 40 - 50, 6000 });
    card.run(7000);
    QCOMPARE(card.beepsHeard.size(), size_t(3)); // 3000, the 3 ms late one, 6000
    QVERIFY(std::abs(card.beepsHeard[0] - 3000) < 0.05);
    QVERIFY(std::abs(card.beepsHeard[2] - 6000) < 0.05);
    QVERIFY(std::abs(card.beepsHeard[1] - 3040) < 0.05);
}

void AudioTest::wavParsing()
{
    // 16-bit mono, 4 samples
    std::vector<unsigned char> b = { 'R', 'I', 'F', 'F', 44, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1,
                                     0, 0x44, 0xAC, 0, 0, 0x88, 0x58, 1, 0, 2, 0, 16, 0, 'd', 'a', 't', 'a', 8, 0, 0, 0, 0, 0, 0,
                                     0x40, 0, 0xC0, 0xFF, 0x7F };
    Audio::Sound s = Audio::parseWav(b);
    QCOMPARE(s.sampleRate, 44100);
    QCOMPARE(s.samples.size(), size_t(4));
    QCOMPARE(s.samples[1], 0.5f);
    QCOMPARE(s.samples[2], -0.5f);
    QVERIFY(Audio::parseWav({ 'n', 'o' }).samples.empty());
}
