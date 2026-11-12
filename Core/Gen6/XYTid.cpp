/*
 * XY Trainer ID RNG: blink seed search, ID list and the opening's blink timeline.
 *
 * This file is shared, identical, by PokeFinderPlus (Core/Gen6) and PokeTimerPlus (Core/Gen6).
 * Copyright (C) 2026 Vict0rious7. GPL-3.0-or-later (see the LICENSE of either project).
 *
 * Method and every timeline value come from BZL's 6genTidSearch (2020), used with his permission:
 * BZL (なるほどえたきちのブログじゃねーの), https://blog.bzl-web.com/entry/2020/08/31/202306
 */

#include "XYTid.hpp"
#include <cstdio>
#include <sstream>

namespace XYTid
{
    namespace
    {
        constexpr u32 MAT1 = 0x8f7011ee;
        constexpr u32 MAT2 = 0xfc78ff1f;
        constexpr u32 TMAT = 0x3793fdff;
    }

    u32 msToFrames(double ms)
    {
        return static_cast<u32>(ms * FPS / 1000.0);
    }

    Tiny Tiny::fromSeed(u32 seed)
    {
        Tiny t;
        t.s = { seed, MAT1, MAT2, TMAT };
        for (u32 i = 1; i < 8; i++)
        {
            t.s[i & 3] ^= i + 0x6c078965 * (t.s[(i - 1) & 3] ^ (t.s[(i - 1) & 3] >> 30));
        }
        if ((t.s[0] & 0x7fffffff) == 0 && t.s[1] == 0 && t.s[2] == 0 && t.s[3] == 0)
        {
            t.s = { 'T', 'I', 'N', 'Y' };
        }
        t.advance(8);
        return t;
    }

    Tiny Tiny::fromState(const std::array<u32, 4> &state)
    {
        Tiny t;
        t.s = state;
        return t;
    }

    void Tiny::nextState()
    {
        u32 y = s[3];
        u32 x = (s[0] & 0x7fffffff) ^ s[1] ^ s[2];
        x ^= x << 1;
        y ^= (y >> 1) ^ x;
        s[0] = s[1];
        s[1] = s[2] ^ ((y & 1) * MAT1);
        s[2] = x ^ (y << 10) ^ ((y & 1) * MAT2);
        s[3] = y;
    }

    u32 Tiny::temper() const
    {
        u32 t0 = s[3];
        u32 t1 = s[0] + (s[2] >> 8);
        t0 ^= t1;
        if (t1 & 1)
        {
            t0 ^= TMAT;
        }
        return t0;
    }

    u32 Tiny::peek() const
    {
        Tiny copy = *this;
        return copy.next();
    }

    namespace
    {
        bool near(u32 interval, u32 gap)
        {
            // unsigned, as in 6genTidSearch (a gap under 10 frames never matches)
            return !(interval > gap + GAP_TOLERANCE) && !(interval < gap - GAP_TOLERANCE);
        }
    }

    Hit hitFrame(std::span<const u32> gaps, u32 seed, u32 advanceMin, u32 advanceMax)
    {
        Hit hit;
        if (gaps.empty())
        {
            return hit;
        }
        const u32 len = static_cast<u32>(gaps.size());
        Tiny mt = Tiny::fromSeed(seed);
        mt.advance(advanceMin);
        u32 matched = 0;
        u32 end = advanceMax + len;
        for (u32 j = advanceMin; j < end; j++)
        {
            u32 interval = blinkInterval(mt.next());
            if (near(interval, gaps[matched]))
            {
                matched++;
            }
            else if (matched != 0)
            {
                matched = near(interval, gaps[0]) ? 1 : 0;
            }
            else
            {
                continue;
            }
            if (matched == len)
            {
                hit.found = true;
                hit.advance = j - len + 1;
                Tiny at = Tiny::fromSeed(seed);
                at.advance(hit.advance);
                hit.state = at.s;
                return hit;
            }
        }
        return hit;
    }

    Timeline::Timeline(const std::array<u32, 4> &state, u32 npc) : mt(Tiny::fromState(state)), npc(npc)
    {
        for (u32 i = 0; i < npc; i++)
        {
            counter[i] = blinkInterval(mt.next()) - START_COUNTER;
            drawn++;
        }
    }

    bool Timeline::processFrame()
    {
        bool professor = false;
        for (u32 i = 0; i < npc; i++)
        {
            if (counter[i] != 0 && --counter[i] != 0)
            {
                continue;
            }
            counter[i] = blinkInterval(mt.next());
            drawn++;
            professor |= i == 0;
        }
        return professor;
    }

    std::vector<Row> waitList(const Timeline &snapshot, bool boy, u32 rows)
    {
        const u32 reset = boy ? STEP4_RESET_BOY : STEP4_RESET_GIRL;
        Timeline t = snapshot;
        t.setNpc(4);
        t.skip(ID_DRAW);
        const u32 base = t.getDrawn();
        std::vector<Row> list;
        list.reserve(rows);
        u32 f = 0;
        for (u32 i = 0; i < rows; i++)
        {
            u32 r = t.peek();
            u32 f2 = f;
            u32 drawn = t.getDrawn();
            do
            {
                t.processFrame();
                f++;
            } while (t.peek() == r);
            Row row;
            row.advance = drawn;
            row.offset = drawn - base;
            row.id = ids(r);
            row.wait = f2 == 0 ? 0 : f2 + reset;
            row.window = f2 == 0 ? f + reset : f - f2;
            list.push_back(row);
        }
        return list;
    }

    Session::Session(const std::array<u32, 4> &state, double elapsedMs) : timeline(state, 1), snapshot(state, 1)
    {
        timeline.skip(msToFrames(elapsedMs) + START_SKIP);
    }

    void Session::advanceTo(u32 frame, std::vector<u32> *blinks)
    {
        while (wall < frame)
        {
            wall++;
            auto range = events.equal_range(wall);
            for (auto it = range.first; it != range.second; ++it)
            {
                switch (it->second)
                {
                case Event::Npc3:
                    timeline.setNpc(3);
                    break;
                case Event::Stop:
                    running = false;
                    break;
                case Event::Draw:
                    timeline.draw();
                    break;
                case Event::Restart:
                    running = true;
                    break;
                case Event::Npc4:
                    timeline.setNpc(4);
                    break;
                case Event::Snapshot:
                    timeline.resetCounter(3);
                    snapshot = timeline;
                    snapshotTaken = true;
                    break;
                }
            }
            events.erase(range.first, range.second);

            int frames = running ? 1 : 0;
            if (auto n = nudges.find(wall); n != nudges.end())
            {
                int extra = n->second;
                nudges.erase(n);
                if (running)
                {
                    frames = 1 + extra;
                    if (frames < 0)
                    {
                        nudges[wall + 1] += frames; // more than one "slower" at once: one frame each
                        frames = 0;
                    }
                }
            }
            for (int i = 0; i < frames; i++)
            {
                if (timeline.processFrame() && blinks)
                {
                    blinks->push_back(wall);
                }
            }
        }
    }

    void Session::nudge(int direction)
    {
        nudges[wall + 1] += direction > 0 ? 1 : -1;
    }

    int Session::step(u32 press, bool isBoy)
    {
        if (steps >= 4)
        {
            return 0;
        }
        if (press < wall)
        {
            press = wall;
        }
        steps++;
        switch (steps)
        {
        case 1:
            events.emplace(press + STEP1_NPC3, Event::Npc3);
            break;
        case 2:
            events.emplace(press + STEP2_STOP, Event::Stop);
            events.emplace(press + STEP2_STOP + 1, Event::Draw);
            break;
        case 3:
            events.emplace(press + STEP3_RESTART, Event::Restart);
            events.emplace(press + STEP3_RESTART + STEP3_NPC4, Event::Npc4);
            break;
        case 4:
            boy = isBoy;
            step4Press = press;
            events.emplace(press + (boy ? STEP4_RESET_BOY : STEP4_RESET_GIRL), Event::Snapshot);
            break;
        }
        return steps;
    }

    std::vector<u32> Session::upcomingBlinks(u32 until) const
    {
        Session copy = *this;
        std::vector<u32> blinks;
        copy.advanceTo(until, &blinks);
        return blinks;
    }

    std::string Handoff::toString() const
    {
        char words[40];
        std::snprintf(words, sizeof(words), "%08X,%08X,%08X,%08X", state[3], state[2], state[1], state[0]);
        std::string text = std::string("PokeTimerPlus XYTID 1 game=") + game + " state=" + words + " tap=" + std::to_string(tapEpochMs);
        if (target)
        {
            text += " target=" + std::to_string(*target);
        }
        if (tid)
        {
            text += " tid=" + std::to_string(*tid);
        }
        if (sid)
        {
            text += " sid=" + std::to_string(*sid);
        }
        return text;
    }

    std::optional<Handoff> Handoff::fromString(const std::string &text)
    {
        std::istringstream in(text);
        std::string a, b, version;
        if (!(in >> a >> b >> version) || a != "PokeTimerPlus" || b != "XYTID" || version != "1")
        {
            return std::nullopt;
        }
        Handoff h;
        bool hasState = false, hasTap = false;
        std::string token;
        try
        {
            while (in >> token)
            {
                auto eq = token.find('=');
                if (eq == std::string::npos)
                {
                    continue;
                }
                std::string key = token.substr(0, eq), value = token.substr(eq + 1);
                if (key == "game" && (value == "X" || value == "Y"))
                {
                    h.game = value[0];
                }
                else if (key == "state")
                {
                    std::array<u32, 4> words {};
                    std::istringstream ws(value);
                    std::string word;
                    int n = 0;
                    while (n < 4 && std::getline(ws, word, ','))
                    {
                        size_t used = 0;
                        unsigned long v = std::stoul(word, &used, 16);
                        if (used != word.size() || word.empty() || word.size() > 8)
                        {
                            return std::nullopt;
                        }
                        words[3 - n++] = static_cast<u32>(v);
                    }
                    if (n != 4)
                    {
                        return std::nullopt;
                    }
                    h.state = words;
                    hasState = true;
                }
                else if (key == "tap")
                {
                    h.tapEpochMs = std::stoll(value);
                    hasTap = true;
                }
                else if (key == "target")
                {
                    h.target = static_cast<u32>(std::stoul(value));
                }
                else if (key == "tid")
                {
                    h.tid = static_cast<u16>(std::stoul(value));
                }
                else if (key == "sid")
                {
                    h.sid = static_cast<u16>(std::stoul(value));
                }
            }
        } catch (const std::exception &)
        {
            return std::nullopt;
        }
        if (!hasState || !hasTap)
        {
            return std::nullopt;
        }
        return h;
    }
}
