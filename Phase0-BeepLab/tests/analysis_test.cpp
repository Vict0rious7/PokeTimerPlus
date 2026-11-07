// Synthetic-recording tests for the beep analysis: we place EonTimer's beep at known times in a
// fake microphone recording (with room echo, noise, optional music, delivery-delayed stamps) and
// check that the analysis recovers latency, jitter, drift, late first beeps and misses.
#include "Analysis.h"
#include "Wav.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>

using namespace beeplab;

static int failures = 0;
#define CHECK(cond, ...)                                                                                     \
    do                                                                                                       \
    {                                                                                                        \
        if (!(cond))                                                                                         \
        {                                                                                                    \
            failures++;                                                                                      \
            std::printf("  FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond);                                     \
            std::printf(__VA_ARGS__);                                                                        \
            std::printf("\n");                                                                               \
        }                                                                                                    \
    } while (0)

struct Scenario
{
    double latencyMs = 40;
    double jitterSdMs = 0.5;
    double outputDriftPpm = 0;
    double firstBeepExtraMs = 0;
    int dropIndex = -1; // -2 = no beeps at all
    bool music = false;
    double noise = 0.002;
    int groups = 6;
    int perGroup = 5;
    double intervalMs = 500;
    double gapMs = 8000;
    unsigned seed = 1;
};

struct Synth
{
    Recording rec;
    std::vector<ScheduledBeep> schedule;
    std::vector<double> trueOffset; // per beep, NaN if dropped
};

static Sound beepSound()
{
    Sound s = loadWav(BEEPLAB_SOUNDS_DIR "/beep.wav");
    return s;
}

static Synth synthesize(const Scenario &sc, const Sound &beep)
{
    const int rate = 48000;
    const double t0 = 100000.0;          // system ms when the run starts
    const double captureStart = t0 - 1000;
    std::mt19937 rng(sc.seed);
    std::normal_distribution<double> gauss(0, 1);
    std::uniform_real_distribution<double> uni(0, 1);

    Synth out;
    double t = t0 + 2000;
    for (int g = 0; g < sc.groups; g++)
    {
        for (int i = 0; i < sc.perGroup; i++)
        {
            out.schedule.push_back({ t, g, i, kNaN });
            t += sc.intervalMs;
        }
        t += sc.gapMs - sc.intervalMs;
    }
    const double endMs = out.schedule.back().ms + 1500;
    const size_t frames = size_t((endMs - captureStart) / 1000.0 * rate);
    out.rec.sampleRate = rate;
    out.rec.samples.assign(frames, 0.0f);

    // "Room": direct sound + two echoes, then a gentle low-pass.
    std::vector<float> b = resample(beep.samples, beep.sampleRate, rate);
    std::vector<float> room(b.size() + 400, 0.0f);
    for (size_t i = 0; i < b.size(); i++)
    {
        room[i] += 0.6f * b[i];
        room[i + 144] += 0.25f * b[i];  // 3 ms
        room[i + 336] += 0.12f * b[i];  // 7 ms
    }
    float lp = 0;
    for (float &v : room)
    {
        lp += 0.5f * (v - lp);
        v = lp;
    }

    for (size_t k = 0; k < out.schedule.size(); k++)
    {
        const auto &s = out.schedule[k];
        if (int(k) == sc.dropIndex || sc.dropIndex == -2)
        {
            out.trueOffset.push_back(kNaN);
            continue;
        }
        double off = sc.latencyMs + sc.outputDriftPpm * 1e-6 * (s.ms - t0) + sc.jitterSdMs * gauss(rng);
        if (s.indexInGroup == 0)
        {
            off += sc.firstBeepExtraMs;
        }
        out.trueOffset.push_back(off);
        double heard = s.ms + off;
        double pos = (heard - captureStart) / 1000.0 * rate;
        size_t p0 = size_t(pos);
        double frac = pos - p0;
        for (size_t i = 0; i + 1 < room.size() && p0 + i + 1 < frames; i++)
        {
            // fractional placement by linear interpolation
            out.rec.samples[p0 + i + 1] += float(room[i] * frac);
            out.rec.samples[p0 + i] += float(room[i] * (1 - frac));
        }
    }

    for (size_t i = 0; i < frames; i++)
    {
        double x = sc.noise * gauss(rng);
        if (sc.music)
        {
            double ts = double(i) / rate;
            double env = 0.6 + 0.4 * std::sin(2 * M_PI * 0.7 * ts);
            x += env * 0.06 * (std::sin(2 * M_PI * 220 * ts) + std::sin(2 * M_PI * 277.2 * ts) +
                               std::sin(2 * M_PI * 329.6 * ts) + 0.5 * std::sin(2 * M_PI * 440 * ts));
            x += 0.03 * std::sin(2 * M_PI * 784 * ts) * (0.5 + 0.5 * std::sin(2 * M_PI * 0.3 * ts)); // near the beep tone
            double beat = std::fmod(ts, 0.4);
            if (beat < 0.03)
            {
                x += 0.08 * gauss(rng) * (1 - beat / 0.03); // drum hit
            }
        }
        out.rec.samples[i] += float(x);
    }

    // Capture chunks every 10 ms, delivered 1..8 ms late.
    for (size_t f = 480; f <= frames; f += 480)
    {
        double captured = captureStart + f * 1000.0 / rate;
        out.rec.stamps.push_back({ int64_t(f), captured + 1 + 7 * uni(rng) });
    }
    return out;
}

// Compares each detected residual with the injected offset after removing the injected mean/drift.
static double worstResidualError(const Synth &s, const RunAnalysis &a)
{
    // Fit the true offsets the same way (line over time) and compare residual by residual.
    std::vector<double> xs, ys;
    for (size_t i = 0; i < s.schedule.size(); i++)
    {
        if (!std::isnan(s.trueOffset[i]) && a.beeps[i].detected)
        {
            xs.push_back(s.schedule[i].ms);
            ys.push_back(s.trueOffset[i]);
        }
    }
    double mx = 0, my = 0;
    for (size_t i = 0; i < xs.size(); i++)
    {
        mx += xs[i];
        my += ys[i];
    }
    mx /= xs.size();
    my /= xs.size();
    double sxx = 0, sxy = 0;
    for (size_t i = 0; i < xs.size(); i++)
    {
        sxx += (xs[i] - mx) * (xs[i] - mx);
        sxy += (xs[i] - mx) * (ys[i] - my);
    }
    double slope = sxy / sxx;
    double worst = 0;
    size_t j = 0;
    for (size_t i = 0; i < s.schedule.size(); i++)
    {
        if (!std::isnan(s.trueOffset[i]) && a.beeps[i].detected)
        {
            double trueRes = ys[j] - (my + slope * (xs[j] - mx));
            worst = std::max(worst, std::abs(trueRes - a.beeps[i].residualMs));
            j++;
        }
    }
    return worst;
}

static RunAnalysis run(const char *name, const Scenario &sc, const Sound &beep, Synth *keep = nullptr)
{
    Synth s = synthesize(sc, beep);
    RunAnalysis a = analyzeScheduled(s.rec, s.schedule, beep.samples, beep.sampleRate);
    const auto &m = a.metrics;
    std::printf("%-22s detected %d/%d latency %.2f jitter %.3f max %.3f first %.2f drift %.1f ppm int.sd %.3f "
                "| worst residual error %.3f ms\n",
                name, m.detected, m.total, m.latencyMs, m.jitterMs, m.maxAbsMs, m.firstBeepDeltaMs, m.driftPpm,
                m.intervalSdMs, worstResidualError(s, a));
    if (keep)
    {
        *keep = s;
    }
    return a;
}

int main()
{
    Sound beep = beepSound();
    CHECK(beep.sampleRate == 44100 && beep.samples.size() == 6615, "beep.wav: %d Hz, %zu frames", beep.sampleRate,
          beep.samples.size());
    if (beep.samples.empty())
    {
        std::printf("cannot load beep.wav\n");
        return 1;
    }

    {
        BeepDetector d(beep.samples, beep.sampleRate, 48000);
        CHECK(std::abs(d.toneHz() - 800) < 20, "tone %.1f Hz", d.toneHz());
    }

    Synth s;
    {
        Scenario sc;
        auto a = run("clean", sc, beep, &s);
        CHECK(a.metrics.detected == a.metrics.total, "all detected");
        CHECK(a.metrics.latencyMs > 35 && a.metrics.latencyMs < 60, "latency %.2f", a.metrics.latencyMs);
        CHECK(a.metrics.jitterMs > 0.3 && a.metrics.jitterMs < 0.8, "jitter %.3f (injected 0.5)", a.metrics.jitterMs);
        CHECK(worstResidualError(s, a) < 0.3, "per-beep residual error %.3f ms", worstResidualError(s, a));
        CHECK(std::abs(a.metrics.driftPpm) < 15, "drift %.1f", a.metrics.driftPpm);
    }
    {
        Scenario sc;
        sc.jitterSdMs = 0;
        auto a = run("perfect", sc, beep, &s);
        CHECK(a.metrics.jitterMs < 0.15, "jitter floor %.3f", a.metrics.jitterMs);
    }
    {
        Scenario sc;
        sc.outputDriftPpm = 100;
        auto a = run("drift 100 ppm", sc, beep, &s);
        CHECK(std::abs(a.metrics.driftPpm - 100) < 15, "drift %.1f", a.metrics.driftPpm);
    }
    {
        Scenario sc;
        sc.firstBeepExtraMs = 30;
        auto a = run("first beep +30 ms", sc, beep, &s);
        CHECK(a.metrics.detected == a.metrics.total, "all detected");
        CHECK(std::abs(a.metrics.firstBeepDeltaMs - 30) < 2, "first delta %.2f", a.metrics.firstBeepDeltaMs);
    }
    {
        Scenario sc;
        sc.dropIndex = 7;
        auto a = run("one beep swallowed", sc, beep, &s);
        CHECK(a.metrics.detected == a.metrics.total - 1, "detected %d", a.metrics.detected);
        CHECK(!a.beeps[7].detected, "beep 7 reported missing");
        CHECK(worstResidualError(s, a) < 0.3, "residual error %.3f", worstResidualError(s, a));
    }
    {
        Scenario sc;
        sc.dropIndex = 0;
        sc.firstBeepExtraMs = 0;
        auto a = run("very first swallowed", sc, beep, &s);
        CHECK(a.metrics.detected == a.metrics.total - 1, "detected %d", a.metrics.detected);
        CHECK(!a.beeps[0].detected, "beep 0 reported missing");
    }
    {
        Scenario sc;
        sc.music = true;
        sc.seed = 7;
        auto a = run("music playing", sc, beep, &s);
        CHECK(a.metrics.detected == a.metrics.total, "detected %d/%d", a.metrics.detected, a.metrics.total);
        CHECK(worstResidualError(s, a) < 0.5, "residual error %.3f", worstResidualError(s, a));
    }
    {
        Scenario sc;
        sc.latencyMs = 250;
        auto a = run("Bluetooth 250 ms", sc, beep, &s);
        CHECK(a.metrics.detected == a.metrics.total, "detected %d", a.metrics.detected);
        CHECK(a.metrics.latencyMs > 245 && a.metrics.latencyMs < 270, "latency %.2f", a.metrics.latencyMs);
    }
    {
        Scenario sc;
        sc.jitterSdMs = 8; // the irregular case
        auto a = run("irregular (8 ms sd)", sc, beep, &s);
        CHECK(a.metrics.detected == a.metrics.total, "detected %d", a.metrics.detected);
        CHECK(a.metrics.jitterMs > 5 && a.metrics.jitterMs < 11, "jitter %.2f", a.metrics.jitterMs);
        CHECK(worstResidualError(s, a) < 0.5, "residual error %.3f", worstResidualError(s, a));
    }

    {
        Scenario sc;
        sc.latencyMs = -35; // engine anchor ahead of reality: beeps come before their scheduled time
        sc.noise = 0;       // digital silence, like a virtual device
        auto a = run("early 35 ms, no noise", sc, beep, &s);
        CHECK(a.metrics.detected == a.metrics.total, "detected %d", a.metrics.detected);
        CHECK(a.metrics.latencyMs < -25 && a.metrics.latencyMs > -40, "latency %.2f", a.metrics.latencyMs);
        CHECK(worstResidualError(s, a) < 0.3, "residual error %.3f", worstResidualError(s, a));
    }
    {
        Scenario sc;
        sc.latencyMs = 480; // almost one interval late: must not be mistaken for the next beep
        auto a = run("late 480 ms", sc, beep, &s);
        CHECK(a.metrics.detected == a.metrics.total, "detected %d", a.metrics.detected);
        CHECK(a.metrics.latencyMs > 475 && a.metrics.latencyMs < 500, "latency %.2f", a.metrics.latencyMs);
    }
    {
        Scenario sc;
        sc.latencyMs = -420; // almost one interval early
        auto a = run("early 420 ms", sc, beep, &s);
        CHECK(a.metrics.detected == a.metrics.total, "detected %d", a.metrics.detected);
        CHECK(a.metrics.latencyMs < -400 && a.metrics.latencyMs > -430, "latency %.2f", a.metrics.latencyMs);
    }
    for (unsigned seed = 11; seed <= 16; seed++)
    {
        Scenario sc;
        sc.music = true;
        sc.seed = seed;
        auto a = run(("music seed " + std::to_string(seed)).c_str(), sc, beep, &s);
        CHECK(a.metrics.detected >= a.metrics.total - 1, "seed %u detected %d", seed, a.metrics.detected);
        CHECK(worstResidualError(s, a) < 1.0, "seed %u residual error %.3f", seed, worstResidualError(s, a));
    }
    {
        Scenario sc;
        sc.music = true;
        sc.dropIndex = -2;
        Synth syn = synthesize(sc, beep);
        auto a = analyzeScheduled(syn.rec, syn.schedule, beep.samples, beep.sampleRate);
        auto l = analyzeListen(syn.rec, 500, beep.samples, beep.sampleRate);
        std::printf("%-22s scheduled run detected %d, listen found %d\n", "music, no beeps", a.metrics.detected,
                    l.metrics.beeps);
        CHECK(a.metrics.detected <= 1, "false detections %d", a.metrics.detected);
        CHECK(l.metrics.beeps <= 2, "false listen events %d", l.metrics.beeps);
    }
    {
        Scenario sc;
        sc.dropIndex = -2;
        Synth syn = synthesize(sc, beep);
        auto a = analyzeScheduled(syn.rec, syn.schedule, beep.samples, beep.sampleRate);
        CHECK(a.metrics.detected == 0, "silence: false detections %d", a.metrics.detected);
    }

    // Listen-only: same signal, schedule unknown to the analysis.
    {
        Scenario sc;
        sc.firstBeepExtraMs = 25; // first interval 25 ms short
        Synth syn = synthesize(sc, beep);
        auto l = analyzeListen(syn.rec, 500, beep.samples, beep.sampleRate);
        const auto &m = l.metrics;
        std::printf("%-22s beeps %d groups %d mean int %.2f sd %.3f max err %.2f first delta %.2f\n", "listen",
                    m.beeps, m.groups, m.meanIntervalMs, m.intervalSdMs, m.intervalMaxAbsErrMs, m.firstIntervalDeltaMs);
        CHECK(m.beeps == sc.groups * sc.perGroup, "beeps %d", m.beeps);
        CHECK(m.groups == sc.groups, "groups %d", m.groups);
        CHECK(std::abs(m.firstIntervalDeltaMs + 25) < 2, "first interval delta %.2f", m.firstIntervalDeltaMs);
    }
    {
        Scenario sc;
        sc.music = true;
        sc.seed = 3;
        Synth syn = synthesize(sc, beep);
        auto l = analyzeListen(syn.rec, 500, beep.samples, beep.sampleRate);
        const auto &m = l.metrics;
        std::printf("%-22s beeps %d groups %d mean int %.2f sd %.3f max err %.2f\n", "listen + music", m.beeps,
                    m.groups, m.meanIntervalMs, m.intervalSdMs, m.intervalMaxAbsErrMs);
        CHECK(m.beeps == sc.groups * sc.perGroup, "beeps %d", m.beeps);
        CHECK(m.intervalMaxAbsErrMs < 3, "max err %.2f", m.intervalMaxAbsErrMs);
    }

    std::printf(failures ? "%d FAILURE(S)\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
