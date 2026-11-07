/*
 * This file is part of PokeTimerPlus
 * Frame rates a timer can count in. EonTimer offers 3 (+ custom), FlowTimer 7; this table has
 * every one with a known source, plus a note on where it comes from and how sure it is.
 */

#ifndef FRAMERATE_HPP
#define FRAMERATE_HPP

#include <string>
#include <string_view>
#include <vector>

/**
 * @brief One selectable frame rate
 */
struct FrameRate
{
    std::string id; // stable key saved in profiles
    std::string name; // shown in the UI
    double fps; // frames per second
    std::string source; // where the value comes from
    bool verified; // false = plausible but not confirmed for RNG use yet
};

namespace FrameRates
{
    /**
     * @brief All built-in frame rates, in display order. "custom" is not in the list.
     */
    const std::vector<FrameRate> &all();

    /**
     * @brief Looks up a frame rate by id
     *
     * @return Pointer into the table, or nullptr if unknown (including "custom")
     */
    const FrameRate *find(std::string_view id);

    /**
     * @brief Frame rate for an id, or customFps when the id is "custom" or unknown
     */
    double fps(std::string_view id, double customFps);

    inline constexpr std::string_view Custom = "custom";
}

#endif // FRAMERATE_HPP
