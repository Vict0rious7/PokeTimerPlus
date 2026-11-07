<p align="center"><img src="Form/Images/Icon/poketimerplus_256x256.png" width="128" alt="PokeTimerPlus logo"></p>

# PokeTimerPlus

All-in-one RNG timer (EonTimer + FlowTimer), in progress. Plan: `Docs/Projects/PokeTimerPlus/PLAN.md` (private, not in this folder).

## Layout (same as PokeFinder)

- `Core/` (no Qt)
  - `Timer/`: frame rates, EonTimer's timer maths, beep schedule (with flowing targets).
  - `Audio/`: the output stream (miniaudio, always open) and `BeepMixer` (beeps at exact sample positions, stalls and clock drift corrected).
  - `Timer/RngTypes`: the RNG types per console tab (GB/GBA, DS, 3DS, Switch) and the frame rates each console offers.
  - `Profile/`: saved calibrations ("Save to profile"), one per RNG type / egg section, JSON.
- `Form/`: the window (Qt Widgets, Fusion, like PokeFinderPlus): console tabs, RNG list, one panel per timer template, timer; EonTimer's beep sounds.
- `Test/`: Qt Test suite. `Resources/eontimer_golden.json` = what EonTimer itself returns on 3,420 cases (made by `Tools/eontimer_golden.py`).
- `Phase0-BeepLab/`: the Phase 0 measurement tool (separate CMake project, see its README). Its Listen mode can time PokeTimerPlus too.
- `LICENSES/`: third-party notices (EonTimer MIT; miniaudio's licence is in `Core/External/miniaudio`).

## Build, test, run on the Mac

```sh
cd ~/Documents/Personnal/Games/Pokemon/Dev/PokeTimerPlus
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DTEST=ON -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DCMAKE_OSX_ARCHITECTURES=$(uname -m)
cmake --build build && (cd build && ctest --output-on-failure)
open build/PokeTimerPlus.app
```

Saved calibrations: `~/Library/Application Support/PokeTimerPlus/PokeTimerPlus/calibrations.json`.

Development aids: `POKETIMERPLUS_SCREENSHOT=<png>` saves the window and quits; `POKETIMERPLUS_AUTOSTART=<ms>` clicks Start after that delay.

## Licence

GPL-3.0 (see `LICENSE`), like PokeFinderPlus. The timer maths are a port of EonTimer (MIT, notice in `LICENSES/`); miniaudio keeps its own licence (`Core/External/miniaudio/LICENSE`).
