// Re-runs the analysis on a saved run: reanalyze <results>/<run> [sound.wav]
// (reads <run>.wav and <run>_stamps.csv, which also holds the schedule). Lets the detection be
// improved later without asking for new recordings.
#include "Analysis.h"
#include "Wav.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace beeplab;

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <run base path> [sound.wav]\n", argv[0]);
        return 2;
    }
    const std::string base = argv[1];
    const std::string soundPath = argc > 2 ? argv[2] : BEEPLAB_SOUNDS_DIR "/beep.wav";
    Sound rec = loadWav(base + ".wav");
    Sound sound = loadWav(soundPath);
    if (rec.samples.empty() || sound.samples.empty())
    {
        std::fprintf(stderr, "cannot read %s.wav or %s\n", base.c_str(), soundPath.c_str());
        return 1;
    }
    Recording r;
    r.sampleRate = rec.sampleRate;
    r.samples = rec.samples;
    std::vector<ScheduledBeep> schedule;
    std::ifstream in(base + "_stamps.csv");
    std::string line;
    while (std::getline(in, line))
    {
        if (line.rfind("# ", 0) == 0 && line.find(',') != std::string::npos)
        {
            ScheduledBeep b;
            char c1, c2;
            std::istringstream ss(line.substr(2));
            ss >> b.ms >> c1 >> b.group >> c2 >> b.indexInGroup;
            schedule.push_back(b);
        }
        else if (!line.empty() && std::isdigit(static_cast<unsigned char>(line[0])))
        {
            Stamp s;
            char c;
            std::istringstream ss(line);
            ss >> s.frames >> c >> s.ms;
            r.stamps.push_back(s);
        }
    }
    auto a = analyzeScheduled(r, schedule, sound.samples, sound.sampleRate);
    const auto &m = a.metrics;
    std::printf("heard %d/%d latency %.2f jitter %.3f max %.3f first %.2f drift %.1f ppm interval sd %.3f max %.3f\n",
                m.detected, m.total, m.latencyMs, m.jitterMs, m.maxAbsMs, m.firstBeepDeltaMs, m.driftPpm,
                m.intervalSdMs, m.intervalMaxAbsMs);
    for (size_t i = 0; i < a.beeps.size(); i++)
    {
        const auto &b = a.beeps[i];
        std::printf("  %2zu g%d/%d %s offset %8.2f residual %7.3f score %.3f\n", i, b.scheduled.group,
                    b.scheduled.indexInGroup, b.detected ? "ok  " : "MISS", b.offsetMs, b.residualMs, b.score);
    }
    return 0;
}
