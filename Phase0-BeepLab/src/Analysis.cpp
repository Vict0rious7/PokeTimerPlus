#include "Analysis.h"

#include "Wav.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace beeplab
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kEnvRate = 6000.0;    // envelope sample rate after decimation
constexpr double kTemplateMs = 60.0;   // length of the envelope template
constexpr double kRefineMs = 10.0;     // after the first acceptable lag, look this far for the peak

double median(std::vector<double> v)
{
    if (v.empty())
    {
        return kNaN;
    }
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

double mean(const std::vector<double> &v)
{
    if (v.empty())
    {
        return kNaN;
    }
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

double sd(const std::vector<double> &v)
{
    if (v.size() < 2)
    {
        return v.empty() ? kNaN : 0.0;
    }
    double m = mean(v);
    double s = 0;
    for (double x : v)
    {
        s += (x - m) * (x - m);
    }
    return std::sqrt(s / (v.size() - 1));
}

double maxAbs(const std::vector<double> &v)
{
    if (v.empty())
    {
        return kNaN;
    }
    double m = 0;
    for (double x : v)
    {
        m = std::max(m, std::abs(x));
    }
    return m;
}

// Strongest frequency of the first 50 ms, scanned 150 Hz..5 kHz.
double dominantFrequency(const std::vector<float> &x, int rate)
{
    size_t n = std::min<size_t>(x.size(), size_t(rate * 0.05));
    if (n < 16)
    {
        return 1000.0;
    }
    double bestF = 1000.0, bestP = -1;
    for (double f = 150; f <= 5000; f += 5)
    {
        double re = 0, im = 0, w = 2 * kPi * f / rate;
        for (size_t i = 0; i < n; i++)
        {
            re += x[i] * std::cos(w * i);
            im -= x[i] * std::sin(w * i);
        }
        double p = re * re + im * im;
        if (p > bestP)
        {
            bestP = p;
            bestF = f;
        }
    }
    return bestF;
}
}

ClockMap fitClock(const std::vector<Stamp> &stamps, int sampleRate)
{
    ClockMap map;
    map.b = 1000.0 / std::max(1, sampleRate);
    std::vector<double> offsets;
    offsets.reserve(stamps.size());
    for (const auto &s : stamps)
    {
        offsets.push_back(s.ms - map.b * double(s.frames));
    }
    map.a = offsets.empty() ? 0.0 : median(offsets);
    return map;
}

double micClockPpm(const std::vector<Stamp> &stamps, int sampleRate)
{
    if (stamps.size() < 10 || sampleRate <= 0)
    {
        return kNaN;
    }
    double mx = 0, my = 0;
    for (const auto &s : stamps)
    {
        mx += double(s.frames);
        my += s.ms;
    }
    mx /= stamps.size();
    my /= stamps.size();
    double sxx = 0, sxy = 0;
    for (const auto &s : stamps)
    {
        double dx = double(s.frames) - mx;
        sxx += dx * dx;
        sxy += dx * (s.ms - my);
    }
    if (sxx <= 0)
    {
        return kNaN;
    }
    return (sxy / sxx / (1000.0 / sampleRate) - 1.0) * 1e6;
}

// ---------------------------------------------------------------- BeepDetector

BeepDetector::BeepDetector(const std::vector<float> &sound, int soundRate, int recordingRate) :
    rate_(recordingRate), decim_(std::max(1, int(std::lround(recordingRate / kEnvRate))))
{
    std::vector<float> s = resample(sound, soundRate, recordingRate);
    toneHz_ = dominantFrequency(s, recordingRate);

    std::vector<float> env = process(s);
    size_t n = std::min(env.size(), size_t(kTemplateMs / 1000.0 * rate_ / decim_));
    tmpl_.assign(env.begin(), env.begin() + n);
    double m = 0;
    for (float v : tmpl_)
    {
        m += v;
    }
    m /= std::max<size_t>(1, tmpl_.size());
    tmplNorm_ = 0;
    for (float &v : tmpl_)
    {
        v = float(v - m);
        tmplNorm_ += double(v) * v;
    }
    tmplNorm_ = std::sqrt(tmplNorm_);
}

// Band-pass (RBJ, Q = 3) around the tone, rectify, one-period moving average, decimate.
std::vector<float> BeepDetector::process(const std::vector<float> &x) const
{
    double w0 = 2 * kPi * toneHz_ / rate_;
    double alpha = std::sin(w0) / (2 * 3.0);
    double a0 = 1 + alpha;
    double b0 = alpha / a0, b2 = -alpha / a0;
    double a1 = -2 * std::cos(w0) / a0, a2 = (1 - alpha) / a0;

    int period = std::max(1, int(std::lround(rate_ / toneHz_)));
    std::vector<float> out;
    out.reserve(x.size() / decim_ + 1);

    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    std::vector<double> ring(period, 0.0);
    double ringSum = 0;
    size_t ringPos = 0;
    double block = 0;
    int blockCount = 0;

    for (float in : x)
    {
        double y = b0 * in + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = in;
        y2 = y1;
        y1 = y;

        double r = std::abs(y);
        ringSum += r - ring[ringPos];
        ring[ringPos] = r;
        ringPos = (ringPos + 1) % period;

        block += ringSum / period;
        if (++blockCount == decim_)
        {
            out.push_back(float(block / decim_));
            block = 0;
            blockCount = 0;
        }
    }
    return out;
}

void BeepDetector::setRecording(const std::vector<float> &samples)
{
    env_ = process(samples);
    sum_.assign(env_.size() + 1, 0.0);
    sum2_.assign(env_.size() + 1, 0.0);
    for (size_t i = 0; i < env_.size(); i++)
    {
        sum_[i + 1] = sum_[i] + env_[i];
        sum2_[i + 1] = sum2_[i] + double(env_[i]) * env_[i];
    }
    // Noise floor: median envelope level, so a run that is mostly silence gives the quiet level.
    std::vector<float> copy = env_;
    if (!copy.empty())
    {
        std::nth_element(copy.begin(), copy.begin() + copy.size() / 2, copy.end());
        floor_ = std::max(1e-9, double(copy[copy.size() / 2]));
    }
}

int64_t BeepDetector::toEnv(double frame) const
{
    return int64_t(std::floor(frame / decim_));
}

double BeepDetector::scoreAt(int64_t k) const
{
    const int64_t n = int64_t(tmpl_.size());
    if (k < 0 || n == 0 || k + n > int64_t(env_.size()) || tmplNorm_ <= 0)
    {
        return 0;
    }
    double s = sum_[k + n] - sum_[k];
    double s2 = sum2_[k + n] - sum2_[k];
    double var = s2 - s * s / n;
    if (var <= 1e-18)
    {
        return 0;
    }
    double dot = 0;
    for (int64_t j = 0; j < n; j++)
    {
        dot += double(env_[k + j]) * tmpl_[j]; // template has zero mean, so the window mean drops out
    }
    return dot / (std::sqrt(var) * tmplNorm_);
}

double BeepDetector::snrAt(int64_t k) const
{
    const int64_t n = int64_t(tmpl_.size());
    if (k < 0 || k + n > int64_t(env_.size()))
    {
        return 0;
    }
    float peak = *std::max_element(env_.begin() + k, env_.begin() + k + n);
    return peak / floor_;
}

bool BeepDetector::accept(int64_t k) const
{
    return scoreAt(k) >= minScore && snrAt(k) >= minSnr;
}

// Parabolic interpolation around the peak score for sub-envelope-sample precision.
BeepDetector::Hit BeepDetector::refine(int64_t k) const
{
    Hit h;
    h.found = true;
    h.score = scoreAt(k);
    h.snr = snrAt(k);
    double ym = scoreAt(k - 1), y0 = h.score, yp = scoreAt(k + 1);
    double denom = ym - 2 * y0 + yp;
    double delta = denom < 0 ? 0.5 * (ym - yp) / denom : 0.0;
    delta = std::clamp(delta, -0.5, 0.5);
    h.frame = (double(k) + delta) * decim_;
    return h;
}

BeepDetector::Hit BeepDetector::best(double fromFrame, double toFrame) const
{
    int64_t a = std::max<int64_t>(0, toEnv(fromFrame));
    int64_t b = std::min<int64_t>(int64_t(env_.size()) - int64_t(tmpl_.size()), toEnv(toFrame));
    // Best score among lags loud enough to be the beep. A quieter stretch (music, or the beep's own
    // decaying tail once the window starts past its peak) can correlate well in shape, so loudness
    // is a filter, not a tie-breaker: the window must hold at least 60% of the loudest peak around.
    double maxSnr = 0;
    for (int64_t k = a; k <= b; k++)
    {
        maxSnr = std::max(maxSnr, snrAt(k));
    }
    const double snrGate = std::max(minSnr, 0.6 * maxSnr);
    int64_t bestK = -1, anyK = -1;
    double bestScore = -2, anyScore = -2;
    for (int64_t k = a; k <= b; k++)
    {
        double s = scoreAt(k);
        if (s > anyScore)
        {
            anyScore = s;
            anyK = k;
        }
        if (s > bestScore && s >= minScore && snrAt(k) >= snrGate)
        {
            bestScore = s;
            bestK = k;
        }
    }
    if (bestK < 0)
    {
        Hit miss;
        if (anyK >= 0)
        {
            miss.score = anyScore;
            miss.snr = snrAt(anyK);
        }
        return miss;
    }
    return refine(bestK);
}

BeepDetector::Hit BeepDetector::earliest(double fromFrame, double toFrame) const
{
    int64_t a = std::max<int64_t>(0, toEnv(fromFrame));
    int64_t b = std::min<int64_t>(int64_t(env_.size()) - int64_t(tmpl_.size()), toEnv(toFrame));
    int64_t span = int64_t(kRefineMs / 1000.0 * rate_ / decim_);
    for (int64_t k = a; k <= b; k++)
    {
        if (accept(k))
        {
            int64_t bestK = k;
            double bestScore = scoreAt(k);
            for (int64_t j = k + 1; j <= std::min(b, k + span); j++)
            {
                double s = scoreAt(j);
                if (s > bestScore)
                {
                    bestScore = s;
                    bestK = j;
                }
            }
            return refine(bestK);
        }
    }
    return {};
}

std::vector<BeepDetector::Hit> BeepDetector::all(double minGapFrames) const
{
    std::vector<Hit> hits;
    double from = 0;
    const double end = double(env_.size()) * decim_;
    while (from < end)
    {
        Hit h = earliest(from, end);
        if (!h.found)
        {
            break;
        }
        hits.push_back(h);
        from = h.frame + minGapFrames;
    }
    return hits;
}

// ---------------------------------------------------------------- scheduled runs

RunAnalysis analyzeScheduled(const Recording &rec, const std::vector<ScheduledBeep> &schedule,
                             const std::vector<float> &sound, int soundRate)
{
    RunAnalysis out;
    RunMetrics &m = out.metrics;
    m.total = int(schedule.size());
    for (const auto &s : schedule)
    {
        BeepResult r;
        r.scheduled = s;
        out.beeps.push_back(r);
    }
    if (rec.samples.empty() || rec.sampleRate <= 0 || schedule.empty())
    {
        return out;
    }

    const ClockMap map = fitClock(rec.stamps, rec.sampleRate);
    m.micClockPpm = micClockPpm(rec.stamps, rec.sampleRate);
    const double fpm = rec.sampleRate / 1000.0; // frames per ms
    BeepDetector det(sound, soundRate, rec.sampleRate);
    det.setRecording(rec.samples);

    // Stage 1: the constant lag between schedule and recording (output + input latency, and any
    // error in the engine's own anchor, which can even be negative). Countdowns are periodic, so a
    // lag one interval off also matches most beeps; it can't match the first AND last beep of every
    // countdown, though. So: try every lag suggested by a (hit, beep) pair in -1000..+3000 ms, keep
    // the one that explains the most beeps (ties: the smallest |lag|).
    std::vector<double> hitFrames;
    for (const auto &h : det.all(80 * fpm))
    {
        hitFrames.push_back(h.frame);
    }
    std::vector<double> expected;
    for (const auto &s : schedule)
    {
        expected.push_back(map.toFrame(s.ms));
    }
    const double tol = 15 * fpm;
    auto explained = [&](double lagFrames) {
        int n = 0;
        for (double fe : expected)
        {
            double t = fe + lagFrames;
            auto it = std::lower_bound(hitFrames.begin(), hitFrames.end(), t - tol);
            if (it != hitFrames.end() && *it <= t + tol)
            {
                n++;
            }
        }
        return n;
    };
    double lag = kNaN;
    int bestCount = 0;
    for (double fe : expected)
    {
        auto lo = std::lower_bound(hitFrames.begin(), hitFrames.end(), fe - 1000 * fpm);
        for (auto it = lo; it != hitFrames.end() && *it <= fe + 3000 * fpm; ++it)
        {
            double l = *it - fe;
            int n = explained(l);
            if (n > bestCount || (n == bestCount && std::abs(l) < std::abs(lag * fpm)))
            {
                bestCount = n;
                lag = l / fpm;
            }
        }
    }
    if (std::isnan(lag))
    {
        return out;
    }

    // Stage 2: each beep, best match within +-W ms of where the typical latency puts it.
    double minSpacing = 1e9;
    for (size_t i = 1; i < schedule.size(); i++)
    {
        minSpacing = std::min(minSpacing, schedule[i].ms - schedule[i - 1].ms);
    }
    const double w = std::min(200.0, 0.45 * minSpacing);

    std::vector<double> xs, offsets;
    for (auto &r : out.beeps)
    {
        double fe = map.toFrame(r.scheduled.ms) + lag * fpm;
        auto h = det.best(fe - w * fpm, fe + w * fpm);
        r.score = h.score;
        r.snr = h.snr;
        if (!h.found)
        {
            continue;
        }
        r.detected = true;
        r.measuredMs = map.toMs(h.frame);
        r.offsetMs = r.measuredMs - r.scheduled.ms;
        xs.push_back(r.scheduled.ms);
        offsets.push_back(r.offsetMs);
    }
    m.detected = int(offsets.size());
    if (offsets.empty())
    {
        return out;
    }
    m.latencyMs = median(offsets);

    // Latency + drift line: offset = c + slope * (t - t_mean).
    double tm = mean(xs), om = mean(offsets);
    double sxx = 0, sxy = 0;
    for (size_t i = 0; i < xs.size(); i++)
    {
        sxx += (xs[i] - tm) * (xs[i] - tm);
        sxy += (xs[i] - tm) * (offsets[i] - om);
    }
    double slope = (xs.size() >= 3 && sxx > 0) ? sxy / sxx : 0.0;
    m.driftPpm = slope * 1e6;

    std::vector<double> residuals, firstRes, otherRes;
    for (auto &r : out.beeps)
    {
        if (!r.detected)
        {
            continue;
        }
        r.residualMs = r.offsetMs - (om + slope * (r.scheduled.ms - tm));
        residuals.push_back(r.residualMs);
        (r.scheduled.indexInGroup == 0 ? firstRes : otherRes).push_back(r.residualMs);
    }
    m.jitterMs = sd(residuals);
    m.maxAbsMs = maxAbs(residuals);
    if (!firstRes.empty() && !otherRes.empty())
    {
        m.firstBeepDeltaMs = mean(firstRes) - mean(otherRes);
    }

    std::vector<double> intervalErr;
    for (size_t i = 1; i < out.beeps.size(); i++)
    {
        const auto &p = out.beeps[i - 1], &c = out.beeps[i];
        if (p.detected && c.detected && p.scheduled.group == c.scheduled.group)
        {
            intervalErr.push_back((c.measuredMs - p.measuredMs) - (c.scheduled.ms - p.scheduled.ms));
        }
    }
    m.intervalSdMs = sd(intervalErr);
    m.intervalMaxAbsMs = maxAbs(intervalErr);

    std::vector<double> late;
    for (const auto &r : out.beeps)
    {
        if (!std::isnan(r.scheduled.triggerMs))
        {
            late.push_back(r.scheduled.triggerMs - r.scheduled.ms);
        }
    }
    if (!late.empty())
    {
        m.triggerLateMeanMs = mean(late);
        m.triggerLateMaxMs = *std::max_element(late.begin(), late.end());
    }
    return out;
}

// ---------------------------------------------------------------- listen-only

ListenAnalysis analyzeListen(const Recording &rec, double nominalIntervalMs, const std::vector<float> &sound,
                             int soundRate)
{
    ListenAnalysis out;
    if (rec.samples.empty() || rec.sampleRate <= 0 || nominalIntervalMs <= 0)
    {
        return out;
    }
    const ClockMap map = fitClock(rec.stamps, rec.sampleRate);
    const double fpm = rec.sampleRate / 1000.0;
    BeepDetector det(sound, soundRate, rec.sampleRate);
    det.setRecording(rec.samples);

    auto hits = det.all(std::min(150.0, 0.4 * nominalIntervalMs) * fpm);
    int group = -1, index = 0;
    double prev = -1e18;
    std::vector<double> firstErr, otherErr, intervals;
    for (const auto &h : hits)
    {
        double t = map.toMs(h.frame);
        double gap = t - prev;
        if (gap > 1.5 * nominalIntervalMs)
        {
            group++;
            index = 0;
        }
        else
        {
            index++;
            double err = gap - nominalIntervalMs;
            out.intervalErrors.push_back(err);
            intervals.push_back(gap);
            (index == 1 ? firstErr : otherErr).push_back(err);
        }
        out.events.push_back({ t, group, index, h.score, h.snr });
        prev = t;
    }
    ListenMetrics &m = out.metrics;
    m.beeps = int(out.events.size());
    m.groups = group + 1;
    m.meanIntervalMs = mean(intervals);
    m.intervalSdMs = sd(intervals);
    m.intervalMaxAbsErrMs = maxAbs(out.intervalErrors);
    if (!firstErr.empty() && !otherErr.empty())
    {
        m.firstIntervalDeltaMs = mean(firstErr) - mean(otherErr);
    }
    return out;
}
}
