/*
 * XY Trainer ID RNG: blink seed search, ID list and the opening's blink timeline.
 *
 * This file is shared, identical, by PokeFinderPlus (Core/Gen6) and PokeTimerPlus (Core/Gen6).
 * Copyright (C) 2026 Vict0rious7. GPL-3.0-or-later (see the LICENSE of either project).
 *
 * Method and every timeline value come from BZL's 6genTidSearch (2020), used with his permission:
 * BZL (なるほどえたきちのブログじゃねーの), https://blog.bzl-web.com/entry/2020/08/31/202306
 * The code follows 6genTidSearch's classes TinyMT, BlinkTimer, BlinkTimeLine and the XY tab's step handlers, and is
 * checked against them (golden data produced by running 6genTidSearch's own code).
 */

#ifndef XYTID_HPP
#define XYTID_HPP

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace XYTid
{
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    /// Frame rate used by 6genTidSearch for the taps and the timeline (DS frame rate, as in FlowTimer for this method)
    constexpr double FPS = 59.8261;

    /// A tapped blink gap matches a TinyMT blink interval within this many frames
    constexpr u32 GAP_TOLERANCE = 10;
    /// Default range of the first tapped blink, in TinyMT outputs from the initial seed
    constexpr u32 DEFAULT_ADVANCE_MIN = 5;
    constexpr u32 DEFAULT_ADVANCE_MAX = 20;

    // Timeline constants (6genTidSearch, XyID step handlers). Frames after the press of each step.
    constexpr u32 START_SKIP = 4; ///< added to the frames elapsed since the first tap when the timeline is built
    constexpr u32 START_COUNTER = 6; ///< the first interval is shortened by this much
    constexpr u32 BEEP_DELAY = 6; ///< a blink is seen (and beeped) this many frames after its interval is drawn
    constexpr u32 STEP1_NPC3 = 6; ///< gender screen: 3 characters blink
    constexpr u32 STEP2_STOP = 28; ///< name input screen: blinking stops, one more TinyMT output is drawn next frame
    constexpr u32 STEP3_RESTART = 136; ///< name decided: blinking restarts
    constexpr u32 STEP3_NPC4 = 146; ///< ... and the protagonist joins this many frames after the restart
    constexpr u32 STEP4_RESET_BOY = 116; ///< name confirmed: the protagonist's counter is reset (boy)
    constexpr u32 STEP4_RESET_GIRL = 136; ///< (girl)
    constexpr u32 ID_DRAW = 660; ///< the ID is drawn this many frames after the snapshot frame (+ the A press)

    /// Frames for a duration in ms, truncated, as 6genTidSearch does
    u32 msToFrames(double ms);

    /// TinyMT with Pokemon's parameters, state words in 6genTidSearch's order (status[0..3])
    struct Tiny
    {
        std::array<u32, 4> s {};

        static Tiny fromSeed(u32 seed);
        static Tiny fromState(const std::array<u32, 4> &state);

        void nextState();
        u32 temper() const;
        u32 next()
        {
            nextState();
            return temper();
        }
        /// The next output without advancing
        u32 peek() const;
        void advance(u32 n)
        {
            for (u32 i = 0; i < n; i++)
            {
                nextState();
            }
        }
    };

    /// Blink interval in frames drawn from one TinyMT output (542 down to 184, even)
    constexpr u32 blinkInterval(u32 r)
    {
        return 542 - static_cast<u32>(((static_cast<u64>(r) * 180) >> 32) << 1);
    }

    struct Ids
    {
        u16 tid;
        u16 sid;
        u16 tsv;
        u8 trv;
    };

    constexpr Ids ids(u32 r)
    {
        u16 tid = r & 0xffff;
        u16 sid = r >> 16;
        return { tid, sid, static_cast<u16>((tid ^ sid) >> 4), static_cast<u8>((tid ^ sid) & 15) };
    }

    struct Hit
    {
        bool found = false;
        u32 advance = 0; ///< TinyMT output (from the initial seed) that gave the first tapped gap
        std::array<u32, 4> state {}; ///< TinyMT state just before that output, i.e. at the first tap
    };

    /**
     * @brief 6genTidSearch's BlinkTimer.HitFrame: does \p seed explain the tapped \p gaps, starting between \p advanceMin and
     * \p advanceMax? Same matching rule (±10 frames, its restart rule after a mismatch included).
     */
    Hit hitFrame(std::span<const u32> gaps, u32 seed, u32 advanceMin, u32 advanceMax);

    /**
     * @brief The opening's blink characters and their TinyMT draws (6genTidSearch's BlinkTimeLine, without its clock)
     */
    class Timeline
    {
    public:
        static constexpr int MAX_NPC = 20;

        /// Builds the timeline at a state with \p npc characters: each draws its first interval, shortened by START_COUNTER
        Timeline(const std::array<u32, 4> &state, u32 npc);

        /// One frame for every character; returns true if the first character (the professor) drew a new interval
        bool processFrame();

        /// Process \p frames frames
        void skip(u32 frames)
        {
            for (u32 i = 0; i < frames; i++)
            {
                processFrame();
            }
        }

        /// One TinyMT output drawn outside the blinks (name input screen)
        void draw()
        {
            mt.next();
            drawn++;
        }

        void setNpc(u32 count)
        {
            npc = count;
        }

        u32 getNpc() const
        {
            return npc;
        }

        void resetCounter(int index)
        {
            counter[index] = 0;
        }

        /// The next TinyMT output (the one an ID would take now)
        u32 peek() const
        {
            return mt.peek();
        }

        /// TinyMT outputs drawn since the state given to the constructor (6genTidSearch's TinyFrame)
        u32 getDrawn() const
        {
            return drawn;
        }

        const std::array<u32, 4> &getState() const
        {
            return mt.s;
        }

        const std::array<u32, MAX_NPC> &getCounters() const
        {
            return counter;
        }

    private:
        Tiny mt;
        std::array<u32, MAX_NPC> counter {};
        u32 npc;
        u32 drawn = 0;
    };

    /// One ID the A press can give after the 4th step
    struct Row
    {
        u32 advance; ///< TinyMT output index, counted from the state at the first tap (same numbering as the Finder)
        u32 offset; ///< advances since the snapshot (6genTidSearch's 消費数 column)
        Ids id;
        u32 wait; ///< frames from the 4th step press to the A press
        u32 window; ///< frames during which the A press gives this ID
    };

    /**
     * @brief 6genTidSearch's 計算 (SYOUHI_CALC): the IDs reachable from the snapshot taken at the 4th step, with the frames
     * to wait from the 4th press. Rows sharing a frame with another draw (unreachable) are not listed.
     */
    std::vector<Row> waitList(const Timeline &snapshot, bool boy, u32 rows);

    /**
     * @brief The opening as it runs: wall frames since the timeline was built (at the handoff), steps and nudges
     *
     * An event due at wall frame w applies before frame w is processed (6genTidSearch's thread and its step tasks race
     * for the same frame; this is one consistent choice).
     */
    class Session
    {
    public:
        /// \p state = TinyMT state at the first tap; \p elapsedMs = time from the first tap to now
        Session(const std::array<u32, 4> &state, double elapsedMs);

        /// Runs up to and including wall frame \p frame; professor draws are appended to \p blinks if given
        void advanceTo(u32 frame, std::vector<u32> *blinks = nullptr);

        u32 getFrame() const
        {
            return wall;
        }

        /// Faster (+1: one extra frame) or slower (-1: one frame not processed), applied at the next frame. Several at
        /// once add up (slower ones take one frame each, as 6genTidSearch's pause-and-restart does).
        void nudge(int direction);

        /**
         * @brief The next step pressed at wall frame \p press (>= the current frame), returns the step number (1-4), 0 if
         * all four were done. \p boy matters for step 4 only.
         */
        int step(u32 press, bool boy);

        int getSteps() const
        {
            return steps;
        }

        /// The 4th step's snapshot exists once the session has run past it
        bool hasSnapshot() const
        {
            return snapshotTaken;
        }

        u32 getStep4Press() const
        {
            return step4Press;
        }

        bool isBoy() const
        {
            return boy;
        }

        const Timeline &getSnapshot() const
        {
            return snapshot;
        }

        const Timeline &getTimeline() const
        {
            return timeline;
        }

        /// Professor blinks (beep frames = these + BEEP_DELAY) from the next frame up to \p until, with the events pending now
        std::vector<u32> upcomingBlinks(u32 until) const;

    private:
        enum class Event
        {
            Npc3,
            Stop,
            Draw,
            Restart,
            Npc4,
            Snapshot
        };

        Timeline timeline;
        Timeline snapshot;
        std::multimap<u32, Event> events;
        std::map<u32, int> nudges;
        u32 wall = 0;
        u32 step4Press = 0;
        int steps = 0;
        bool running = true;
        bool boy = true;
        bool snapshotTaken = false;
    };

    /// Local socket name PokeTimerPlus listens on for "Send to PokeTimerPlus"
    constexpr const char *SOCKET_NAME = "PokeTimerPlus-XYTID";

    /**
     * @brief What PokeFinderPlus hands to PokeTimerPlus: one line of text, sent over the local socket or the clipboard
     *
     * Example: "PokeTimerPlus XYTID 1 game=X state=0A1B2C3D,11223344,55667788,99AABBCC tap=1794482303966 target=165
     * tid=12345 sid=54321". State words are written status[3] first, as 6genTidSearch and the Finder's table show them.
     */
    struct Handoff
    {
        char game = 'X'; ///< 'X' or 'Y'
        std::array<u32, 4> state {}; ///< TinyMT state at the first tap (status[0..3])
        long long tapEpochMs = 0; ///< system clock time of the first tap, ms since 1970
        std::optional<u32> target; ///< advance of the ID picked in the Finder
        std::optional<u16> tid;
        std::optional<u16> sid;

        std::string toString() const;

        /// Reads a line made by toString (extra spaces and unknown keys ignored); nullopt if it isn't one
        static std::optional<Handoff> fromString(const std::string &text);
    };
}

#endif // XYTID_HPP
