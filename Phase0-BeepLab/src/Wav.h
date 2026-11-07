// Minimal WAV loading and resampling. Pure C++, no Qt, so the analysis tests can use it.
#pragma once

#include <string>
#include <vector>

namespace beeplab
{
struct Sound
{
    int sampleRate = 0;
    std::vector<float> samples; // mono, -1..1
};

// Parses a RIFF/WAVE buffer (PCM 8/16/24/32-bit or 32-bit float). Channels are averaged to mono.
// Returns an empty Sound on failure.
Sound parseWav(const std::vector<unsigned char> &bytes);
Sound loadWav(const std::string &path);

// Writes mono 16-bit PCM. Returns false on I/O error.
bool saveWav16(const std::string &path, const std::vector<float> &samples, int sampleRate);

// Linear-interpolation resampler. Good enough for short beeps and for analysis templates.
std::vector<float> resample(const std::vector<float> &in, int inRate, int outRate);
}
