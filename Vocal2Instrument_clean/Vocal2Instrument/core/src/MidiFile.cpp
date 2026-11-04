#include "v2i/MidiFile.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace v2i
{
namespace
{
void putVlq (std::vector<uint8_t>& t, uint32_t v)
{
    uint8_t tmp[5]; int n = 0;
    tmp[n++] = v & 0x7F;
    while ((v >>= 7) != 0) tmp[n++] = (uint8_t) ((v & 0x7F) | 0x80);
    while (n--) t.push_back (tmp[n]);
}
void put32 (std::vector<uint8_t>& o, uint32_t v) { for (int s = 24; s >= 0; s -= 8) o.push_back ((uint8_t) (v >> s)); }
void put16 (std::vector<uint8_t>& o, uint16_t v) { o.push_back ((uint8_t) (v >> 8)); o.push_back ((uint8_t) v); }

struct Ev { uint32_t tick; int order; uint8_t status, d1, d2; };   // order: 0 = note off first, 1 = note on
}

std::vector<uint8_t> writeMidiFile (const std::vector<MidiNote>& notes, double bpm, int gmProgram,
                                    const std::string& trackName, int channel)
{
    std::vector<Ev> evs;
    for (const auto& n : notes)
    {
        const uint32_t on = (uint32_t) std::max (0L, std::lround (n.startBeat * kMidiPpq));
        uint32_t off = (uint32_t) std::max (0L, std::lround ((n.startBeat + n.lengthBeat) * kMidiPpq));
        if (off <= on) off = on + 1;
        const uint8_t note = (uint8_t) std::max (0, std::min (127, n.note)), vel = (uint8_t) std::max (1, std::min (127, n.velocity));
        evs.push_back ({ on, 1, (uint8_t) (0x90 | channel), note, vel });
        evs.push_back ({ off, 0, (uint8_t) (0x80 | channel), note, 0 });
    }
    std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : a.order < b.order; });

    std::vector<uint8_t> t;
    // tempo
    const uint32_t mpqn = (uint32_t) std::lround (60000000.0 / std::max (1.0, bpm));
    putVlq (t, 0); t.insert (t.end(), { 0xFF, 0x51, 0x03, (uint8_t) (mpqn >> 16), (uint8_t) (mpqn >> 8), (uint8_t) mpqn });
    // time signature 4/4
    putVlq (t, 0); t.insert (t.end(), { 0xFF, 0x58, 0x04, 4, 2, 24, 8 });
    // track name
    putVlq (t, 0); t.push_back (0xFF); t.push_back (0x03); putVlq (t, (uint32_t) trackName.size()); t.insert (t.end(), trackName.begin(), trackName.end());
    // program change
    putVlq (t, 0); t.push_back ((uint8_t) (0xC0 | channel)); t.push_back ((uint8_t) std::max (0, std::min (127, gmProgram)));

    uint32_t last = 0;
    for (const auto& e : evs) { putVlq (t, e.tick - last); last = e.tick; t.push_back (e.status); t.push_back (e.d1); t.push_back (e.d2); }
    putVlq (t, 0); t.insert (t.end(), { 0xFF, 0x2F, 0x00 });

    std::vector<uint8_t> f = { 'M', 'T', 'h', 'd' };
    put32 (f, 6); put16 (f, 0); put16 (f, 1); put16 (f, (uint16_t) kMidiPpq);
    f.insert (f.end(), { 'M', 'T', 'r', 'k' });
    put32 (f, (uint32_t) t.size());
    f.insert (f.end(), t.begin(), t.end());
    return f;
}

bool saveMidiFile (const std::string& path, const std::vector<uint8_t>& bytes)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return false;
    const bool ok = std::fwrite (bytes.data(), 1, bytes.size(), f) == bytes.size();
    std::fclose (f);
    return ok;
}

bool parseMidiFile (const std::vector<uint8_t>& b, ParsedMidi& out)
{
    if (b.size() < 22 || std::string (b.begin(), b.begin() + 4) != "MThd") return false;
    out.ppq = (b[12] << 8) | b[13];
    size_t pos = 14;
    std::map<int, std::pair<uint32_t, int>> active;   // note -> (startTick, velocity)
    while (pos + 8 <= b.size())
    {
        const std::string id (b.begin() + (long) pos, b.begin() + (long) pos + 4);
        const uint32_t len = ((uint32_t) b[pos + 4] << 24) | (b[pos + 5] << 16) | (b[pos + 6] << 8) | b[pos + 7];
        pos += 8;
        if (id != "MTrk") { pos += len; continue; }
        const size_t end = std::min (b.size(), pos + len);
        uint32_t tick = 0; uint8_t running = 0;
        auto vlq = [&] { uint32_t v = 0; while (pos < end) { uint8_t c = b[pos++]; v = (v << 7) | (c & 0x7F); if (! (c & 0x80)) break; } return v; };
        while (pos < end)
        {
            tick += vlq();
            uint8_t st = b[pos];
            if (st & 0x80) { ++pos; if (st < 0xF0) running = st; } else st = running;
            if (st == 0xFF)
            {
                const uint8_t type = b[pos++]; const uint32_t l = vlq();
                if (type == 0x51 && l == 3) out.bpm = 60000000.0 / (double) ((b[pos] << 16) | (b[pos + 1] << 8) | b[pos + 2]);
                if (type == 0x03) out.trackName.assign (b.begin() + (long) pos, b.begin() + (long) pos + l);
                pos += l;
                if (type == 0x2F) break;
            }
            else if ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0) { if ((st & 0xF0) == 0xC0) out.program = b[pos]; ++pos; }
            else if (st >= 0xF0) { const uint32_t l = vlq(); pos += l; }
            else
            {
                const uint8_t d1 = b[pos], d2 = b[pos + 1]; pos += 2;
                const bool on = (st & 0xF0) == 0x90 && d2 > 0, off = (st & 0xF0) == 0x80 || ((st & 0xF0) == 0x90 && d2 == 0);
                if (on) active[d1] = { tick, d2 };
                else if (off && active.count (d1))
                {
                    auto a = active[d1]; active.erase (d1);
                    out.notes.push_back ({ (double) a.first / out.ppq, (double) (tick - a.first) / out.ppq, d1, a.second });
                }
            }
        }
    }
    std::sort (out.notes.begin(), out.notes.end(), [] (const MidiNote& a, const MidiNote& c) { return a.startBeat < c.startBeat; });
    return true;
}
}
