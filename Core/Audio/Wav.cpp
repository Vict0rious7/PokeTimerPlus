#include "Wav.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>

namespace Audio
{
namespace
{
uint32_t le32(const unsigned char *p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

uint16_t le16(const unsigned char *p)
{
    return uint16_t(p[0] | (p[1] << 8));
}
}

Sound parseWav(const std::vector<unsigned char> &b)
{
    Sound out;
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "WAVE", 4) != 0)
    {
        return out;
    }

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const unsigned char *data = nullptr;
    size_t dataSize = 0;

    size_t pos = 12;
    while (pos + 8 <= b.size())
    {
        const unsigned char *chunk = b.data() + pos;
        uint32_t size = le32(chunk + 4);
        size_t body = pos + 8;
        if (body + size > b.size())
        {
            size = uint32_t(b.size() - body);
        }
        if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16)
        {
            format = le16(b.data() + body);
            channels = le16(b.data() + body + 2);
            rate = le32(b.data() + body + 4);
            bits = le16(b.data() + body + 14);
            if (format == 0xFFFE && size >= 26) // WAVE_FORMAT_EXTENSIBLE: real format in the sub-format GUID
            {
                format = le16(b.data() + body + 24);
            }
        }
        else if (std::memcmp(chunk, "data", 4) == 0)
        {
            data = b.data() + body;
            dataSize = size;
        }
        pos = body + size + (size & 1);
    }

    if (!data || channels == 0 || rate == 0 || (format != 1 && format != 3))
    {
        return out;
    }

    const size_t bytesPerSample = bits / 8;
    if (bytesPerSample == 0)
    {
        return out;
    }
    const size_t frames = dataSize / (bytesPerSample * channels);
    out.sampleRate = int(rate);
    out.samples.resize(frames);

    for (size_t f = 0; f < frames; f++)
    {
        double sum = 0;
        for (size_t c = 0; c < channels; c++)
        {
            const unsigned char *p = data + (f * channels + c) * bytesPerSample;
            double v = 0;
            if (format == 3 && bits == 32)
            {
                float x;
                std::memcpy(&x, p, 4);
                v = x;
            }
            else if (bits == 8)
            {
                v = (int(p[0]) - 128) / 128.0;
            }
            else if (bits == 16)
            {
                v = int16_t(le16(p)) / 32768.0;
            }
            else if (bits == 24)
            {
                int32_t x = int32_t(uint32_t(p[0]) << 8 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 24) >> 8;
                v = x / 8388608.0;
            }
            else if (bits == 32)
            {
                v = int32_t(le32(p)) / 2147483648.0;
            }
            sum += v;
        }
        out.samples[f] = float(sum / channels);
    }
    return out;
}

Sound loadWav(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return {};
    }
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parseWav(bytes);
}

bool saveWav16(const std::string &path, const std::vector<float> &samples, int sampleRate)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        return false;
    }
    auto put32 = [&](uint32_t v) {
        char b[4] = { char(v), char(v >> 8), char(v >> 16), char(v >> 24) };
        out.write(b, 4);
    };
    auto put16 = [&](uint16_t v) {
        char b[2] = { char(v), char(v >> 8) };
        out.write(b, 2);
    };
    const uint32_t dataBytes = uint32_t(samples.size() * 2);
    out.write("RIFF", 4);
    put32(36 + dataBytes);
    out.write("WAVEfmt ", 8);
    put32(16);
    put16(1);
    put16(1);
    put32(uint32_t(sampleRate));
    put32(uint32_t(sampleRate) * 2);
    put16(2);
    put16(16);
    out.write("data", 4);
    put32(dataBytes);
    for (float v : samples)
    {
        float c = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
        put16(uint16_t(int16_t(c * 32767.0f)));
    }
    return bool(out);
}

std::vector<float> resample(const std::vector<float> &in, int inRate, int outRate)
{
    if (in.empty() || inRate <= 0 || outRate <= 0 || inRate == outRate)
    {
        return in;
    }
    const double step = double(inRate) / outRate;
    const size_t n = size_t(double(in.size()) / step);
    std::vector<float> out(n);
    for (size_t i = 0; i < n; i++)
    {
        double x = i * step;
        size_t j = size_t(x);
        double frac = x - j;
        float a = in[j];
        float b = j + 1 < in.size() ? in[j + 1] : 0.0f;
        out[i] = float(a + (b - a) * frac);
    }
    return out;
}
}
