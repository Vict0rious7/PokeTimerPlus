/*
 * This file is part of PokeTimerPlus
 */

#include "RngTypes.hpp"

#include <algorithm>

namespace RngTypes
{
    const std::vector<Console> &consoles()
    {
        static const std::vector<Console> list = {
            { "gba", "GB / GBA", { "gba", "nds-slot2", "open-agb-firm", "gbi", "switch-frlg" } },
            { "ds", "DS", { "nds-slot1" } },
            { "3ds", "3DS", { "3ds", "60" } },
            { "switch", "Switch", { "60", "30" } },
        };
        return list;
    }

    const std::vector<RngType> &all()
    {
        static const std::vector<RngSection> one = { { "", "", "" } };
        static const std::vector<RngType> list = {
            // GB / GBA
            { "rse-wild", "gba", "RSE Wild", TimerTemplate::Gen3Frames, "rse-wild", one },
            { "rse-static", "gba", "RSE Static", TimerTemplate::Gen3Frames, "rse-static", one },
            { "rse-fishing", "gba", "RSE Fishing", TimerTemplate::Gen3Frames, "rse-fishing", one },
            { "e-egg",
              "gba",
              "Emerald Egg",
              TimerTemplate::Gen3Frames,
              "e-egg",
              { { "held", "Held", "Decides the PID and shininess. Can only be calibrated with Spinda eggs." },
                { "pickup", "Pickup", "Decides the IVs. Calibrated like a static." } } },
            { "rs-egg", "gba", "RS Egg", TimerTemplate::Gen3Frames, "rs-egg", { { "held", "Held", "" }, { "pickup", "Pickup", "" } } },
            { "frlg-static", "gba", "FRLG Static", TimerTemplate::SeedAdvances, "frlg-static", one },
            { "frlg-wild", "gba", "FRLG Wild", TimerTemplate::SeedAdvances, "frlg-wild", one },
            { "frlg-egg", "gba", "FRLG Egg", TimerTemplate::SeedAdvances, "frlg-egg", { { "held", "Held", "" }, { "pickup", "Pickup", "" } } },
            { "custom-gba", "gba", "Custom", TimerTemplate::Custom, "custom-gba", one },
            // DS
            { "dppt-static", "ds", "DPPt Static", TimerTemplate::Gen4Delay, "dppt-static", one },
            { "dppt-wild", "ds", "DPPt Wild", TimerTemplate::Gen4Delay, "dppt-wild", one },
            { "bw", "ds", "BW / B2W2", TimerTemplate::Gen5, "gen5", one },
            { "custom-ds", "ds", "Custom", TimerTemplate::Custom, "custom-ds", one },
            // 3DS, Switch: custom until those games are covered
            { "custom-3ds", "3ds", "Custom", TimerTemplate::Custom, "custom-3ds", one },
            { "custom-switch", "switch", "Custom", TimerTemplate::Custom, "custom-switch", one },
        };
        return list;
    }

    std::vector<const RngType *> forConsole(std::string_view console)
    {
        std::vector<const RngType *> out;
        for (const auto &t : all())
        {
            if (t.console == console)
            {
                out.push_back(&t);
            }
        }
        return out;
    }

    const RngType *find(std::string_view id)
    {
        const auto &list = all();
        auto it = std::find_if(list.begin(), list.end(), [id](const RngType &t) { return t.id == id; });
        return it == list.end() ? nullptr : &*it;
    }

    std::string calibrationKey(const RngType &type, const RngSection &section)
    {
        return section.id.empty() ? type.calibrationKey : type.calibrationKey + "/" + section.id;
    }
}
