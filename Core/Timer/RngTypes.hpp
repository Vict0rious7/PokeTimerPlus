/*
 * This file is part of PokeTimerPlus
 * The RNG types the window offers, grouped by console tab (Victor's list, 2026-11-05). Each type uses
 * one timer template and keeps its own saved calibration (Gen 5 shares one across all methods:
 * Smogon's BW guides tie it to the DS and game, not the method).
 */

#ifndef RNGTYPES_HPP
#define RNGTYPES_HPP

#include <string>
#include <string_view>
#include <vector>

enum class TimerTemplate
{
    Gen3Frames, // EonTimer Gen 3: pre-timer + target frame, calibration in ms
    SeedAdvances, // FRLG: initial seed time (ms) + advances, each with its own calibration
    Gen4Delay, // EonTimer Gen 4: target delay + second, calibrated delay + second
    Gen5, // EonTimer Gen 5 modes
    Custom // free phases, or continuous (targets added while the timer runs)
};

struct RngSection
{
    std::string id; // "held", "pickup"; empty for single-section types
    std::string name;
    std::string note; // short hint shown under the fields (empty = none)
};

struct RngType
{
    std::string id;
    std::string console; // "gba", "ds", "3ds", "switch"
    std::string name;
    TimerTemplate timer;
    std::string calibrationKey; // where the calibration is saved; per section: key + "/" + section id
    std::vector<RngSection> sections; // at least one
};

struct Console
{
    std::string id;
    std::string name;
    std::vector<std::string> frameRates; // FrameRates ids offered as "Played on", first = default; "custom" always added
};

namespace RngTypes
{
    const std::vector<Console> &consoles();
    const std::vector<RngType> &all();
    std::vector<const RngType *> forConsole(std::string_view console);
    const RngType *find(std::string_view id);
    std::string calibrationKey(const RngType &type, const RngSection &section);
}

#endif // RNGTYPES_HPP
