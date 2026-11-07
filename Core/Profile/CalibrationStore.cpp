/*
 * This file is part of PokeTimerPlus
 */

#include "CalibrationStore.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

bool CalibrationStore::has(const std::string &key) const
{
    return calibrations.contains(key) || customPhases.contains(key);
}

std::optional<double> CalibrationStore::get(const std::string &key, const std::string &field) const
{
    auto it = calibrations.find(key);
    if (it == calibrations.end())
    {
        return std::nullopt;
    }
    auto f = it->second.find(field);
    return f == it->second.end() ? std::nullopt : std::optional<double>(f->second);
}

void CalibrationStore::set(const std::string &key, const std::map<std::string, double> &fields)
{
    calibrations[key] = fields;
}

namespace CalibrationFile
{
    std::string toJson(const CalibrationStore &store)
    {
        json custom = json::object();
        for (const auto &[key, phases] : store.customPhases)
        {
            json list = json::array();
            for (const auto &p : phases)
            {
                list.push_back({ { "unit", std::string(toString(p.unit)) }, { "target", p.target }, { "calibration", p.calibration } });
            }
            custom[key] = list;
        }
        json j = { { "version", store.version },
                   { "calibrations", store.calibrations },
                   { "customPhases", custom },
                   { "hardware", store.hardware },
                   { "customFps", store.customFps } };
        return j.dump(2);
    }

    CalibrationStore fromJson(const std::string &text, std::string *error)
    {
        CalibrationStore store;
        json j = json::parse(text, nullptr, false);
        if (j.is_discarded() || !j.is_object())
        {
            if (error)
            {
                *error = "not a valid calibration file";
            }
            return store;
        }
        auto object = [&](const char *key) -> const json * {
            auto it = j.find(key);
            return it != j.end() && it->is_object() ? &*it : nullptr;
        };
        if (const json *c = object("calibrations"))
        {
            for (auto it = c->begin(); it != c->end(); ++it)
            {
                if (!it->is_object())
                {
                    continue;
                }
                std::map<std::string, double> fields;
                for (auto f = it->begin(); f != it->end(); ++f)
                {
                    if (f->is_number() && std::isfinite(f->get<double>()))
                    {
                        fields[f.key()] = f->get<double>();
                    }
                }
                store.calibrations[it.key()] = fields;
            }
        }
        if (const json *c = object("customPhases"))
        {
            for (auto it = c->begin(); it != c->end(); ++it)
            {
                if (!it->is_array())
                {
                    continue;
                }
                std::vector<CustomPhase> phases;
                for (const auto &p : *it)
                {
                    if (!p.is_object())
                    {
                        continue;
                    }
                    CustomPhase phase;
                    phase.unit = customUnitFromString(p.value("unit", std::string())).value_or(phase.unit);
                    if (p.contains("target") && p["target"].is_number())
                    {
                        phase.target = p["target"].get<int>();
                    }
                    if (p.contains("calibration") && p["calibration"].is_number())
                    {
                        phase.calibration = p["calibration"].get<double>();
                    }
                    phases.push_back(phase);
                }
                store.customPhases[it.key()] = phases;
            }
        }
        if (const json *h = object("hardware"))
        {
            for (auto it = h->begin(); it != h->end(); ++it)
            {
                if (it->is_string())
                {
                    store.hardware[it.key()] = it->get<std::string>();
                }
            }
        }
        if (const json *f = object("customFps"))
        {
            for (auto it = f->begin(); it != f->end(); ++it)
            {
                if (it->is_number() && it->get<double>() > 0)
                {
                    store.customFps[it.key()] = it->get<double>();
                }
            }
        }
        return store;
    }

    CalibrationStore load(const std::string &path, std::string *error)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            return {};
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return fromJson(ss.str(), error);
    }

    bool save(const std::string &path, const CalibrationStore &store, std::string *error)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path target(path);
        if (target.has_parent_path())
        {
            fs::create_directories(target.parent_path(), ec);
        }
        fs::path tmp = target;
        tmp += ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out << toJson(store) << "\n";
            if (!out)
            {
                if (error)
                {
                    *error = "cannot write " + tmp.string();
                }
                return false;
            }
        }
        fs::rename(tmp, target, ec);
        if (ec)
        {
            if (error)
            {
                *error = "cannot replace " + target.string() + ": " + ec.message();
            }
            return false;
        }
        return true;
    }
}
