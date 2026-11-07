/*
 * This file is part of PokeTimerPlus
 * Saved calibrations ("Save to profile"): one set per RNG type (and per egg section), used to
 * pre-fill the calibration fields. A ballpark, not per output device (Victor, 2026-11-04).
 */

#ifndef CALIBRATIONSTORE_HPP
#define CALIBRATIONSTORE_HPP

#include <Core/Timer/Timers.hpp>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct CalibrationStore
{
    int version = 2;
    // calibration key (RngTypes) -> field -> value
    std::map<std::string, std::map<std::string, double>> calibrations;
    // custom timers: key -> saved phases (with their calibration)
    std::map<std::string, std::vector<CustomPhase>> customPhases;
    // console id -> chosen frame rate id ("Played on"), and the custom fps
    std::map<std::string, std::string> hardware;
    std::map<std::string, double> customFps;

    bool has(const std::string &key) const;
    std::optional<double> get(const std::string &key, const std::string &field) const;
    void set(const std::string &key, const std::map<std::string, double> &fields);
};

namespace CalibrationFile
{
    std::string toJson(const CalibrationStore &store);
    CalibrationStore fromJson(const std::string &text, std::string *error = nullptr);
    CalibrationStore load(const std::string &path, std::string *error = nullptr); // missing file = empty store
    bool save(const std::string &path, const CalibrationStore &store, std::string *error = nullptr); // atomic
}

#endif // CALIBRATIONSTORE_HPP
