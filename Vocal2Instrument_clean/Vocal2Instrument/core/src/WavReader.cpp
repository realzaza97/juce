#include "v2i/WavReader.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace v2i
{
static uint32_t rd32 (const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24); }
static uint16_t rd16 (const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); }

bool readWavFile (const std::string& path, AudioData& out, std::string& error)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (! f) { error = "cannot open " + path; return false; }
    std::fseek (f, 0, SEEK_END);
    long size = std::ftell (f);
    std::fseek (f, 0, SEEK_SET);
    std::vector<uint8_t> d ((size_t) std::max (0L, size));
    if (size <= 0 || std::fread (d.data(), 1, d.size(), f) != d.size()) { std::fclose (f); error = "read error"; return false; }
    std::fclose (f);

    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) != 0 || std::memcmp (d.data() + 8, "WAVE", 4) != 0)
    { error = "not a RIFF/WAVE file"; return false; }

    int fmtTag = 0, channels = 0, bits = 0; uint32_t sr = 0;
    const uint8_t* data = nullptr; size_t dataLen = 0;
    size_t pos = 12;
    while (pos + 8 <= d.size())
    {
        uint32_t len = rd32 (&d[pos + 4]);
        const uint8_t* body = &d[pos + 8];
        size_t avail = d.size() - (pos + 8);
        if (std::memcmp (&d[pos], "fmt ", 4) == 0 && len >= 16 && avail >= 16)
        {
            fmtTag = rd16 (body); channels = rd16 (body + 2); sr = rd32 (body + 4); bits = rd16 (body + 14);
            if (fmtTag == 0xFFFE && len >= 26 && avail >= 26) fmtTag = rd16 (body + 24); // sub-format GUID first 2 bytes
        }
        else if (std::memcmp (&d[pos], "data", 4) == 0)
        {
            data = body; dataLen = std::min<size_t> (len, avail);
            break;
        }
        pos += 8 + (size_t) len + (len & 1);
    }
    if (! data || channels < 1 || sr == 0) { error = "missing fmt/data chunk"; return false; }
    const bool isFloat = (fmtTag == 3);
    if (! ((fmtTag == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (isFloat && (bits == 32 || bits == 64))))
    { error = "unsupported WAV encoding"; return false; }

    const int bps = bits / 8;
    const size_t frames = dataLen / ((size_t) bps * channels);
    out.sampleRate = sr;
    out.samples.assign (frames, 0.f);
    for (size_t i = 0; i < frames; ++i)
    {
        double acc = 0;
        for (int c = 0; c < channels; ++c)
        {
            const uint8_t* p = data + (i * channels + c) * bps;
            double v = 0;
            if (isFloat)           { if (bits == 32) { float x; std::memcpy (&x, p, 4); v = x; } else { double x; std::memcpy (&x, p, 8); v = x; } }
            else if (bits == 8)    v = (p[0] - 128) / 128.0;
            else if (bits == 16)   v = (int16_t) rd16 (p) / 32768.0;
            else if (bits == 24)   { int32_t x = (p[0] << 8) | (p[1] << 16) | ((int32_t) p[2] << 24); v = (x >> 8) / 8388608.0; }
            else                   v = (int32_t) rd32 (p) / 2147483648.0;
            acc += v;
        }
        out.samples[i] = (float) (acc / channels);
    }
    return true;
}

bool writeWavFile16 (const std::string& path, const AudioData& in)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return false;
    const uint32_t n = (uint32_t) in.samples.size(), sr = (uint32_t) in.sampleRate;
    auto w32 = [&] (uint32_t v) { uint8_t b[4] = { (uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) }; std::fwrite (b, 1, 4, f); };
    auto w16 = [&] (uint16_t v) { uint8_t b[2] = { (uint8_t) v, (uint8_t) (v >> 8) }; std::fwrite (b, 1, 2, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + n * 2); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (1); w32 (sr); w32 (sr * 2); w16 (2); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (n * 2);
    for (float s : in.samples) w16 ((uint16_t) (int16_t) std::lround (std::max (-1.f, std::min (1.f, s)) * 32767.f));
    std::fclose (f);
    return true;
}
}
