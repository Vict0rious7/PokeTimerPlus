/*
 * This file is part of PokeTimerPlus
 */

#include "FrameRate.hpp"

#include <algorithm>

namespace FrameRates
{
    const std::vector<FrameRate> &all()
    {
        // The three EonTimer values are its exact constants (59.7275 rather than 16777216 / 280896,
        // a 1e-8 relative difference), so calibrations carry over unchanged.
        static const std::vector<FrameRate> table = {
            { "gba", "GBA / GB / GBC", 59.7275, "16,777,216 / 280,896 (EonTimer, FlowTimer, retailrng)", true },
            { "nds-slot2", "GBA game on DS / DS Lite (slot 2)", 59.6555, "DS GBA mode (EonTimer, FlowTimer, retailrng)", true },
            { "nds-slot1", "DS game on DS / DSi / 3DS", 59.8261, "DS native (EonTimer, FlowTimer)", true },
            { "open-agb-firm", "GBA on 3DS (open_agb_firm)", 59.6555, "retailrng: use NDS slot 2", false },
            { "gbi", "Game Boy Interface (GameCube)", 59.7275, "retailrng: identical to a real GBA", false },
            { "3ds", "3DS game (native)", 59.834, "Citra's measured 3DS refresh rate", false },
            { "switch-frlg", "Switch: FireRed / LeafGreen", 119.445, "Mystic Timer constant (2026-07-28), unexplained", false },
            { "ntsc", "GameCube / Wii (NTSC)", 60.0 / 1.001, "NTSC video, 60 / 1.001", false },
            { "pal", "GameCube / Wii (PAL)", 50.0, "PAL video", false },
            { "60", "60", 60.0, "FlowTimer list; Switch", false },
            { "30", "30", 30.0, "FlowTimer list: games updating every 2nd frame", false },
            { "25", "25", 25.0, "FlowTimer list: half of PAL", false },
            { "15", "15", 15.0, "FlowTimer list", false },
        };
        return table;
    }

    const FrameRate *find(std::string_view id)
    {
        const auto &table = all();
        auto it = std::find_if(table.begin(), table.end(), [id](const FrameRate &f) { return f.id == id; });
        return it == table.end() ? nullptr : &*it;
    }

    double fps(std::string_view id, double customFps)
    {
        const FrameRate *f = find(id);
        return f ? f->fps : customFps;
    }
}
